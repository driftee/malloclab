/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "invictus_gaming",
    /* First member's full name */
    "Liguoxiang",
    /* First member's email address */
    "2018202135@ruc.edu.cn",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/*
 * simple explicit free list
 * 
 * allocated block
 *  ________
 * |header  |(4)
 * |--------|
 * |        |
 * |payload |  
 * |        |
 * |________|
 * |footer  |(4)
 * |________|
 * 
 * free block
 *  ________
 * |header  |(4)
 * |--------|
 * |next    |(4)
 * |--------|  
 * |prev    |(4)
 * |--------|
 * |...     |(This place can be empty, thus the minimum size of a free block is 16 bytes)
 * |--------|
 * |footer  |(4)
 * |________|
 * 
 * 
 */


/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))


#define COMMANDER ()

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    // p acts as a guard in the head of heap which can be accessed via mem_heap_lo().
    // When the guard is not 0, it points to a free block. Then the whole free list can be accessed.
    size_t **p = mem_sbrk(4);
    *p = 0;
    return 0;


    // printf("Ini called %d\n", times);

}

/*
 * A function for debug.
 * You can run it in GDB.
 * It prints out the whole free list.
 */
void check_free_list()
{
    printf("Start checking free list...\n");
    size_t *start = mem_heap_lo();
    if (!*start)
    { // the guard is 0 so the free list is empty
        printf("The free list is currently empty.\n");
        return;
    }
    size_t *first = (size_t *)*start;
    int i = 0;
    while (1)
    {
        i++;
        size_t size = *(first - 1) & ~3;
        printf("Free block #%d of %ld byte(s) from 0x%lx to 0x%lx\n", i, (long)size, (unsigned long)first, (unsigned long)(first + size / 4));
        first = (size_t *)*first;
        if (first == (size_t *)*start)
        {
            break;
        }
    }
    printf("Checking done.\n");
}


/*
 * Delete a freeblock from the free list.
 */
void delete_free_block(size_t *ptr)
{
    size_t *after = (size_t *)*ptr;
    size_t *first = mem_heap_lo();
    if (after == ptr)
    { // The free list contains a free block only
        *first = 0;
        return;
    }
    size_t *before = (size_t *)*(ptr + 1);
    *(after + 1) = (size_t)before;
    *before = (size_t)after;
    if (*first == (size_t)ptr)
    { // If the guard points to the block to be deleted, then change the guard.
        *first = (size_t)after;
    }
}


/*
 * Set boundary tag for allocated blocks
 */
void set_alloc_boundary_tag(size_t *ptr, size_t size)
{
    *(ptr - 1) = size | 1;
    *(ptr + size / 4 - 2) = size | 1;
}

/*
 * Set boundary tag for free blocks
 */
void set_free_boundary_tag(size_t *ptr, size_t size)
{
    *(ptr - 1) = size;
    *(ptr + size / 4 - 2) = size;
}

/*
 * Insert new free block into the free list
 */
void insert_free_block(size_t *ptr)
{
    size_t *first = (size_t *)mem_heap_lo();
    if(!*first)
    { // empty free list
        *first = (size_t)ptr;
        *ptr = (size_t)ptr;
        *(ptr + 1) = (size_t)ptr;
        return;
    }
    size_t *next = (size_t *)*first;

    size_t *block_before = (size_t *)*(next + 1);
    *(next + 1) = (size_t)ptr;
    *block_before = (size_t)ptr;
    *ptr = (size_t)next;
    *(ptr + 1) = (size_t)block_before;

    // *first = (size_t)ptr;
}

/*
 * Assign the old free block to the new and set the size
 */
void assign_free_block(size_t *old, size_t *new, size_t newsize)
{
    size_t *first = mem_heap_lo();
    set_free_boundary_tag(new, newsize);
    if (*old == (size_t)old)
    { // The freelist contains one free block only
        *first = (size_t) new;
        *(new + 1) = (size_t) new;
        *new = (size_t) new;
        return;
    }
    if (*first == (size_t)old)
    {
        *first = (size_t) new;
    }
    size_t *prev = (size_t *)*(old + 1);
    size_t *next = (size_t *)*old;
    *prev = (size_t) new;
    *(next + 1) = (size_t) new;
    *new = (size_t)next;
    *(new + 1) = (size_t)prev;
}

/*
 * Scan in the free list to find the free block BEST for newsize.
 * Return a pointer to a block with allocated boundary tag set.(Return 0 if no block is found)
 * The free list changes accordingly.
 */
size_t *scan_free_block(size_t newsize)
{
    size_t *first = (size_t *)mem_heap_lo();
    size_t *current = (size_t *)*first;
    if (!current)
    { // empty free list
        return 0;
    }
    size_t min_quan = ~0;
    size_t* bestfit = 0;
    int fit_flag = 0;
    while (1)
    {
        size_t free_size = *(current - 1);
        if (newsize <= free_size - 16)
        {
            if(min_quan > free_size - newsize)
            {
                fit_flag = 1;
                min_quan = free_size - newsize;
                bestfit = current;
            }

        }
        if(newsize <= free_size && newsize > free_size - 16)
        {
            set_alloc_boundary_tag(current, free_size);
            delete_free_block(current);
            return current;
        }

        current = (size_t *)*current;
        if (*first == (size_t)current)
        {
            break;
            // return 0;
        }
    }
    if(!fit_flag)
    {
        return 0;
    }

    size_t free_size  = *(bestfit - 1);
    size_t left = free_size - newsize;
    set_alloc_boundary_tag(bestfit, newsize);
    assign_free_block(bestfit, bestfit + newsize / 4, left);
    return bestfit;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    // printf("\n");
    // printf("Applying for %ld bytes. \n", size);

    size_t newsize = ALIGN(size + SIZE_T_SIZE);

    // Seek in the free list first
    size_t *current = scan_free_block(newsize);
    if (current != 0)
    {
        // printf("Get pointer %lx from freelist.\n", (size_t)current);
        return (void *)current;
    }

    // Cannot find a fine block from the free list
    size_t last_byte = (size_t)mem_heap_hi() - 3;
    size_t *pointer = (size_t *)last_byte;
    if (!(*pointer & 1) && ((size_t)pointer > (size_t)mem_heap_lo()))
    { // The last block in the heap is empty but it's not large enough, then we can expand it to a extent so that it can contain what it have to
        int free_size = *pointer & ~3;
        current = pointer - free_size / 4 + 2;
        mem_sbrk(newsize - free_size);
        // pointer_checker(current, newsize, free_size);
        set_alloc_boundary_tag(current, newsize);
        delete_free_block(current);
        // printf("Get pointer %lx by partly sbrk.\n", (size_t)current);
        return current;
    }

    // No other means but sbrk
    size_t *p = mem_sbrk(newsize);
    if (p == (void *)(-1))
    {
        return NULL;
    }
    set_alloc_boundary_tag(p + 1, newsize);
    // printf("Get pointer %lx by totally sbrk.\n", (size_t)(p + 1));
    return p + 1;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    if (ptr == NULL)
    {
        return;
    }
    // printf("Freeing pointer %lx\n", (size_t)ptr);

    size_t *current = (size_t *)ptr;
    size_t free_size = *(current - 1) & ~3;
    // size_t another_size = *(current - 2) & ~3;
    // if(another_size > free_size)
    // {
    //     my_pause(free_size);
    //     current -= 1;
    //     free_size = another_size;
    // }
    // printf("the size to be freed is %ld\n", free_size);
    
    
    int flag_before = 0;

    if (!(*(current - 2) & 1) && (current - 2) > (size_t *)mem_heap_lo())
    { // Coalescing with the free block before

        // printf("Combining with the free block before\n");
        size_t prev_size = *(current - 2) & ~3;

        current -= prev_size / 4; // current指针指向合并后的block
        free_size += prev_size;

        // check_free_list();
        flag_before = 1;
    }
    if (!(*(current + free_size / 4 - 1) & 1) && (current + free_size / 4 - 1) <= (size_t *)((size_t)mem_heap_hi() - 15))
    { // Coalescing with the free block after
        size_t next_size = *(current + free_size / 4 - 1) & ~3;

        // delete it from free list
        delete_free_block(current + free_size / 4);

        free_size += next_size;
    }

    if (!flag_before)
    { // If the freed block didn't coalesce with the block before, then add it to the free list
        insert_free_block(current);
    }

    set_free_boundary_tag(current, free_size);
}


/*
 * My memcpy.
 */
void mm_copy(size_t *old, size_t *new)
{
    size_t copysize = (*(old - 1) & ~3) / 4;
    for(int i = 0; i < copysize; i ++)
    {
        *(new + i) = *(old + i);
    }
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{

    // printf("Realloc called\n");
    size_t original_size = size;
    size = ALIGN(size + SIZE_T_SIZE);
    size_t *current = (size_t *)ptr;
    size_t old_size = *(current - 1) & ~3;
    if(size <= old_size - 16)
    {
        size_t left = old_size - size;
        set_alloc_boundary_tag(current, size);
        if(!(*(current + old_size / 4 - 1) & 1))
        {
            assign_free_block(current + old_size / 4, current + size / 4, left);
        }
        else
        {
            insert_free_block(current + size / 4);
        }
        set_free_boundary_tag(current + size / 4, left);
        return (void *)current;
    }
    if(size > old_size - 16 && size <= old_size - 8)
    {
        if (!(*(current + old_size / 4 - 1) & 1))
        {
            size_t left = old_size - size;
            assign_free_block(current + old_size / 4, current + size / 4, left);
        }
        else
        {
            return (void *)current;
        }
    }
    if (size > old_size - 8 && size <= old_size)
    {
        return (void *)current;
    }

    if(size > old_size)
    {
        size_t need_size = size - old_size;
        if((size_t)(current + old_size / 4 - 1) == ((size_t)mem_heap_hi()) + 1)
        { // the block to be realloced is at the end of the heap
            mem_sbrk(need_size);
            set_alloc_boundary_tag(current, size);
            return current;
        }
        if (!(*(current + old_size / 4 - 1) & 1) && (size_t)(current + old_size / 4 - 1) <= ((size_t)mem_heap_hi()) - 15)
        {
            size_t free_size = *(current + old_size / 4 - 1) & ~3;
            if(need_size <= free_size - 16)
            { 
                set_alloc_boundary_tag(current, size);
                assign_free_block(current + old_size / 4, current + size / 4, free_size - need_size);
                return current;
            }

            if(need_size <= free_size && need_size > free_size - 16)
            {
                set_alloc_boundary_tag(current, old_size + free_size);
                delete_free_block(current + old_size / 4);
                return current;
            }
        }
        
    }

    // No other means but for malloc and free
    size_t *newptr = mm_malloc(original_size);
    if(current != newptr)
        mm_copy(current, newptr);
    mm_free(current);
    return (void *)newptr;
}
