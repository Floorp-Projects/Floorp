/* -*- Mode: C -*-
  ======================================================================
  FILE: icalmemory.c
  CREATOR: eric 30 June 1999
  
  $Id: icalmemory.c,v 1.10 2002/08/09 14:28:56 lindner Exp $
  $Locker:  $
    
 The contents of this file are subject to the Mozilla Public License
 Version 1.0 (the "License"); you may not use this file except in
 compliance with the License. You may obtain a copy of the License at
 http://www.mozilla.org/MPL/
 
 Software distributed under the License is distributed on an "AS IS"
 basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 the License for the specific language governing rights and
 limitations under the License.
 

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

 The Original Code is icalmemory.h

 ======================================================================*/

/**
 * @file icalmemory.c
 * @brief Common memory management routines.
 *
 * libical often passes strings back to the caller. To make these
 * interfaces simple, I did not want the caller to have to pass in a
 * memory buffer, but having libical pass out newly allocated memory
 * makes it difficult to de-allocate the memory.
 * 
 * The ring buffer in this scheme makes it possible for libical to pass
 * out references to memory which the caller does not own, and be able
 * to de-allocate the memory later. The ring allows libical to have
 * several buffers active simultaneously, which is handy when creating
 * string representations of components. 
 */

#define ICALMEMORY_C

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#include "icalmemory.h"
#include "icalerror.h"

#include <stdio.h> /* for printf (debugging) */
#include <stdlib.h> /* for malloc, realloc */
#include <string.h> /* for memset(), strdup */

#ifdef WIN32
#include <windows.h>
#endif

#define BUFFER_RING_SIZE 2500
#define MIN_BUFFER_SIZE 200


/* HACK. Not threadsafe */

typedef struct {
	int pos;
	void *ring[BUFFER_RING_SIZE];
} buffer_ring;

void icalmemory_free_tmp_buffer (void* buf);
void icalmemory_free_ring_byval(buffer_ring *br);

static buffer_ring* global_buffer_ring = 0;

#ifdef HAVE_PTHREAD
#include <pthread.h>

static pthread_key_t  ring_key;
static pthread_once_t ring_key_once = PTHREAD_ONCE_INIT;

static void ring_destroy(void * buf) {
    if (buf) icalmemory_free_ring_byval((buffer_ring *) buf);
    pthread_setspecific(ring_key, NULL);
}

static void ring_key_alloc(void) {  
    pthread_key_create(&ring_key, ring_destroy);
}
#endif


static buffer_ring * buffer_ring_new(void) {
	buffer_ring *br;
	int i;

	br = (buffer_ring *)malloc(sizeof(buffer_ring));

	for(i=0; i<BUFFER_RING_SIZE; i++){
	    br->ring[i]  = 0;
	}
	br->pos = 0;
        return(br);
}


#ifdef HAVE_PTHREAD
static buffer_ring* get_buffer_ring_pthread(void) {
    buffer_ring *br;

    pthread_once(&ring_key_once, ring_key_alloc);

    br = pthread_getspecific(ring_key);

    if (!br) {
	br = buffer_ring_new();
	pthread_setspecific(ring_key, br);
    }
    return(br);
}
#endif

/* get buffer ring via a single global for a non-threaded program */
static buffer_ring* get_buffer_ring_global(void) {
    if (global_buffer_ring == 0) {
	global_buffer_ring = buffer_ring_new();
    }
    return(global_buffer_ring);
}

static buffer_ring *get_buffer_ring(void) {
#ifdef HAVE_PTHREAD
	return(get_buffer_ring_pthread());
#else
	return get_buffer_ring_global();
#endif
}


/** Add an existing buffer to the buffer ring */
void  icalmemory_add_tmp_buffer(void* buf)
{
    buffer_ring *br = get_buffer_ring();


    /* Wrap around the ring */
    if(++(br->pos) == BUFFER_RING_SIZE){
	br->pos = 0;
    }

    /* Free buffers as their slots are overwritten */
    if ( br->ring[br->pos] != 0){
	free( br->ring[br->pos]);
    }

    /* Assign the buffer to a slot */
    br->ring[br->pos] = buf; 
}


/**
 * Create a new temporary buffer on the ring. Libical owns these and
 * will deallocate them. 
 */

void*
icalmemory_tmp_buffer (size_t size)
{
    char *buf;

    if (size < MIN_BUFFER_SIZE){
	size = MIN_BUFFER_SIZE;
    }
    
    buf = (void*)malloc(size);

    if( buf == 0){
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }

    memset(buf,0,size); 

    icalmemory_add_tmp_buffer(buf);

    return buf;
}

/** get rid of this buffer ring */
void icalmemory_free_ring_byval(buffer_ring *br) {
   int i;
   for(i=0; i<BUFFER_RING_SIZE; i++){
    if ( br->ring[i] != 0){
       free( br->ring[i]);
    }
    }
   free(br);
}

void icalmemory_free_ring()
{
   buffer_ring *br;
   br = get_buffer_ring();

   icalmemory_free_ring_byval(br);
}



/** Like strdup, but the buffer is on the ring. */
char*
icalmemory_tmp_copy(const char* str)
{
    char* b = icalmemory_tmp_buffer(strlen(str)+1);

    strcpy(b,str);

    return b;
}
    

char* icalmemory_strdup(const char *s)
{
    return strdup(s);
}

void
icalmemory_free_tmp_buffer (void* buf)
{
   if(buf == 0)
   {
       return;
   }

   free(buf);
}


/*
 * These buffer routines create memory the old fashioned way -- so the
 * caller will have to deallocate the new memory 
 */

void* icalmemory_new_buffer(size_t size)
{
    void *b = malloc(size);

    if( b == 0){
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }

    memset(b,0,size); 

    return b;
}

void* icalmemory_resize_buffer(void* buf, size_t size)
{
    void *b = realloc(buf, size);

    if( b == 0){
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }

   return b;
}

void icalmemory_free_buffer(void* buf)
{
    free(buf);
}

void 
icalmemory_append_string(char** buf, char** pos, size_t* buf_size, 
			      const char* string)
{
    char *new_buf;
    char *new_pos;

    size_t data_length, final_length, string_length;

#ifndef ICAL_NO_INTERNAL_DEBUG
    icalerror_check_arg_rv( (buf!=0),"buf");
    icalerror_check_arg_rv( (*buf!=0),"*buf");
    icalerror_check_arg_rv( (pos!=0),"pos");
    icalerror_check_arg_rv( (*pos!=0),"*pos");
    icalerror_check_arg_rv( (buf_size!=0),"buf_size");
    icalerror_check_arg_rv( (*buf_size!=0),"*buf_size");
    icalerror_check_arg_rv( (string!=0),"string");
#endif 

    string_length = strlen(string);
    data_length = (size_t)*pos - (size_t)*buf;    
    final_length = data_length + string_length; 

    if ( final_length >= (size_t) *buf_size) {

	
	*buf_size  = (*buf_size) * 2  + final_length;

	new_buf = realloc(*buf,*buf_size);

	new_pos = (void*)((size_t)new_buf + data_length);
	
	*pos = new_pos;
	*buf = new_buf;
    }
    
    strcpy(*pos, string);

    *pos += string_length;
}


void 
icalmemory_append_char(char** buf, char** pos, size_t* buf_size, 
			      char ch)
{
    char *new_buf;
    char *new_pos;

    size_t data_length, final_length;

#ifndef ICAL_NO_INTERNAL_DEBUG
    icalerror_check_arg_rv( (buf!=0),"buf");
    icalerror_check_arg_rv( (*buf!=0),"*buf");
    icalerror_check_arg_rv( (pos!=0),"pos");
    icalerror_check_arg_rv( (*pos!=0),"*pos");
    icalerror_check_arg_rv( (buf_size!=0),"buf_size");
    icalerror_check_arg_rv( (*buf_size!=0),"*buf_size");
#endif

    data_length = (size_t)*pos - (size_t)*buf;

    final_length = data_length + 2; 

    if ( final_length > (size_t) *buf_size ) {

	
	*buf_size  = (*buf_size) * 2  + final_length +1;

	new_buf = realloc(*buf,*buf_size);

	new_pos = (void*)((size_t)new_buf + data_length);
	
	*pos = new_pos;
	*buf = new_buf;
    }

    **pos = ch;
    *pos += 1;
    **pos = 0;
}
