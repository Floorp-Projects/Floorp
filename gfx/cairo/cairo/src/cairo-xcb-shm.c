/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2007 Chris Wilson
 * Copyright © 2009 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributors(s):
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

#include "cairoint.h"

/**
 * CAIRO_HAS_XCB_SHM_FUNCTIONS:
 *
 * Defined if Cairo has SHM functions for XCB.
 * This macro can be used to conditionally compile backend-specific code.
 *
 * Since: 1.10
 **/

#if CAIRO_HAS_XCB_SHM_FUNCTIONS

#include "cairo-xcb-private.h"
#include "cairo-list-inline.h"
#include "cairo-mempool-private.h"

#include <xcb/shm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>

#define CAIRO_MAX_SHM_MEMORY (16*1024*1024)

/* a simple buddy allocator for memory pools
 * XXX fragmentation? use Doug Lea's malloc?
 */

typedef struct _cairo_xcb_shm_mem_block cairo_xcb_shm_mem_block_t;

typedef enum {
    PENDING_WAIT,
    PENDING_POLL
} shm_wait_type_t;

struct _cairo_xcb_shm_mem_pool {
    int shmid;
    uint32_t shmseg;
    void *shm;

    cairo_mempool_t mem;

    cairo_list_t link;
};

static void
_cairo_xcb_shm_mem_pool_destroy (cairo_xcb_shm_mem_pool_t *pool)
{
    cairo_list_del (&pool->link);

    shmdt (pool->shm);
    _cairo_mempool_fini (&pool->mem);

    free (pool);
}

static void
_cairo_xcb_shm_info_finalize (cairo_xcb_shm_info_t *shm_info)
{
    cairo_xcb_connection_t *connection = shm_info->connection;

    assert (CAIRO_MUTEX_IS_LOCKED (connection->shm_mutex));

    _cairo_mempool_free (&shm_info->pool->mem, shm_info->mem);
    _cairo_freepool_free (&connection->shm_info_freelist, shm_info);

    /* scan for old, unused pools - hold at least one in reserve */
    if (! cairo_list_is_singular (&connection->shm_pools))
    {
	cairo_xcb_shm_mem_pool_t *pool, *next;
	cairo_list_t head;

	cairo_list_init (&head);
	cairo_list_move (connection->shm_pools.next, &head);

	cairo_list_foreach_entry_safe (pool, next, cairo_xcb_shm_mem_pool_t,
				       &connection->shm_pools, link)
	{
	    if (pool->mem.free_bytes == pool->mem.max_bytes) {
		_cairo_xcb_connection_shm_detach (connection, pool->shmseg);
		_cairo_xcb_shm_mem_pool_destroy (pool);
	    }
	}

	cairo_list_move (head.next, &connection->shm_pools);
    }
}

static void
_cairo_xcb_shm_process_pending (cairo_xcb_connection_t *connection, shm_wait_type_t wait)
{
    cairo_xcb_shm_info_t *info, *next;
    xcb_get_input_focus_reply_t *reply;

    assert (CAIRO_MUTEX_IS_LOCKED (connection->shm_mutex));
    cairo_list_foreach_entry_safe (info, next, cairo_xcb_shm_info_t,
				   &connection->shm_pending, pending)
    {
	switch (wait) {
	case PENDING_WAIT:
	     reply = xcb_wait_for_reply (connection->xcb_connection,
					 info->sync.sequence, NULL);
	     break;
	case PENDING_POLL:
	    if (! xcb_poll_for_reply (connection->xcb_connection,
				      info->sync.sequence,
				      (void **) &reply, NULL))
		/* We cannot be sure the server finished with this image yet, so
		 * try again later. All other shm info are guaranteed to have a
		 * larger sequence number and thus don't have to be checked. */
		return;
	    break;
	default:
	    /* silence Clang static analyzer warning */
	    ASSERT_NOT_REACHED;
	    reply = NULL;
	}

	free (reply);
	cairo_list_del (&info->pending);
	_cairo_xcb_shm_info_finalize (info);
    }
}

cairo_int_status_t
_cairo_xcb_connection_allocate_shm_info (cairo_xcb_connection_t *connection,
					 size_t size,
					 cairo_bool_t might_reuse,
					 cairo_xcb_shm_info_t **shm_info_out)
{
    cairo_xcb_shm_info_t *shm_info;
    cairo_xcb_shm_mem_pool_t *pool, *next;
    size_t bytes, maxbits = 16, minbits = 8;
    size_t shm_allocated = 0;
    void *mem = NULL;
    cairo_status_t status;

    assert (connection->flags & CAIRO_XCB_HAS_SHM);

    CAIRO_MUTEX_LOCK (connection->shm_mutex);
    _cairo_xcb_shm_process_pending (connection, PENDING_POLL);

    if (might_reuse) {
	cairo_list_foreach_entry (shm_info, cairo_xcb_shm_info_t,
		&connection->shm_pending, pending) {
	    if (shm_info->size >= size) {
		cairo_list_del (&shm_info->pending);
		CAIRO_MUTEX_UNLOCK (connection->shm_mutex);

		xcb_discard_reply (connection->xcb_connection, shm_info->sync.sequence);
		shm_info->sync.sequence = XCB_NONE;

		*shm_info_out = shm_info;
		return CAIRO_STATUS_SUCCESS;
	    }
	}
    }

    cairo_list_foreach_entry_safe (pool, next, cairo_xcb_shm_mem_pool_t,
				   &connection->shm_pools, link)
    {
	if (pool->mem.free_bytes > size) {
	    mem = _cairo_mempool_alloc (&pool->mem, size);
	    if (mem != NULL) {
		/* keep the active pools towards the front */
		cairo_list_move (&pool->link, &connection->shm_pools);
		goto allocate_shm_info;
	    }
	}
	/* scan for old, unused pools */
	if (pool->mem.free_bytes == pool->mem.max_bytes) {
	    _cairo_xcb_connection_shm_detach (connection,
					      pool->shmseg);
	    _cairo_xcb_shm_mem_pool_destroy (pool);
	} else {
	    shm_allocated += pool->mem.max_bytes;
	}
    }

    if (unlikely (shm_allocated >= CAIRO_MAX_SHM_MEMORY)) {
	CAIRO_MUTEX_UNLOCK (connection->shm_mutex);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    pool = _cairo_malloc (sizeof (cairo_xcb_shm_mem_pool_t));
    if (unlikely (pool == NULL)) {
	CAIRO_MUTEX_UNLOCK (connection->shm_mutex);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    bytes = 1 << maxbits;
    while (bytes <= size)
	bytes <<= 1, maxbits++;
    bytes <<= 3;

    do {
	pool->shmid = shmget (IPC_PRIVATE, bytes, IPC_CREAT | 0600);
	if (pool->shmid != -1)
	    break;

	/* If the allocation failed because we asked for too much memory, we try
	 * again with a smaller request, as long as our allocation still fits. */
	bytes >>= 1;
	if (errno != EINVAL || bytes < size)
	    break;
    } while (TRUE);
    if (pool->shmid == -1) {
	int err = errno;
	if (! (err == EINVAL || err == ENOMEM))
	    connection->flags &= ~CAIRO_XCB_HAS_SHM;
	free (pool);
	CAIRO_MUTEX_UNLOCK (connection->shm_mutex);
	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    pool->shm = shmat (pool->shmid, NULL, 0);
    if (unlikely (pool->shm == (char *) -1)) {
	shmctl (pool->shmid, IPC_RMID, NULL);
	free (pool);
	CAIRO_MUTEX_UNLOCK (connection->shm_mutex);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    status = _cairo_mempool_init (&pool->mem, pool->shm, bytes,
				  minbits, maxbits - minbits + 1);
    if (unlikely (status)) {
	shmdt (pool->shm);
	free (pool);
	CAIRO_MUTEX_UNLOCK (connection->shm_mutex);
	return status;
    }

    pool->shmseg = _cairo_xcb_connection_shm_attach (connection, pool->shmid, FALSE);
    shmctl (pool->shmid, IPC_RMID, NULL);

    cairo_list_add (&pool->link, &connection->shm_pools);
    mem = _cairo_mempool_alloc (&pool->mem, size);

  allocate_shm_info:
    shm_info = _cairo_freepool_alloc (&connection->shm_info_freelist);
    if (unlikely (shm_info == NULL)) {
	_cairo_mempool_free (&pool->mem, mem);
	CAIRO_MUTEX_UNLOCK (connection->shm_mutex);
	return _cairo_error (CAIRO_STATUS_NO_MEMORY);
    }

    shm_info->connection = connection;
    shm_info->pool = pool;
    shm_info->shm = pool->shmseg;
    shm_info->size = size;
    shm_info->offset = (char *) mem - (char *) pool->shm;
    shm_info->mem = mem;
    shm_info->sync.sequence = XCB_NONE;

    /* scan for old, unused pools */
    cairo_list_foreach_entry_safe (pool, next, cairo_xcb_shm_mem_pool_t,
				   &connection->shm_pools, link)
    {
	if (pool->mem.free_bytes == pool->mem.max_bytes) {
	    _cairo_xcb_connection_shm_detach (connection,
					      pool->shmseg);
	    _cairo_xcb_shm_mem_pool_destroy (pool);
	}
    }
    CAIRO_MUTEX_UNLOCK (connection->shm_mutex);

    *shm_info_out = shm_info;
    return CAIRO_STATUS_SUCCESS;
}

void
_cairo_xcb_shm_info_destroy (cairo_xcb_shm_info_t *shm_info)
{
    cairo_xcb_connection_t *connection = shm_info->connection;

    /* We can only return shm_info->mem to the allocator when we can be sure
     * that the X server no longer reads from it. Since the X server processes
     * requests in order, we send a GetInputFocus here.
     * _cairo_xcb_shm_process_pending () will later check if the reply for that
     * request was received and then actually mark this memory area as free. */

    CAIRO_MUTEX_LOCK (connection->shm_mutex);
    assert (shm_info->sync.sequence == XCB_NONE);
    shm_info->sync = xcb_get_input_focus (connection->xcb_connection);

    cairo_list_init (&shm_info->pending);
    cairo_list_add_tail (&shm_info->pending, &connection->shm_pending);
    CAIRO_MUTEX_UNLOCK (connection->shm_mutex);
}

void
_cairo_xcb_connection_shm_mem_pools_flush (cairo_xcb_connection_t *connection)
{
    CAIRO_MUTEX_LOCK (connection->shm_mutex);
    _cairo_xcb_shm_process_pending (connection, PENDING_WAIT);
    CAIRO_MUTEX_UNLOCK (connection->shm_mutex);
}

void
_cairo_xcb_connection_shm_mem_pools_fini (cairo_xcb_connection_t *connection)
{
    assert (cairo_list_is_empty (&connection->shm_pending));
    while (! cairo_list_is_empty (&connection->shm_pools)) {
	_cairo_xcb_shm_mem_pool_destroy (cairo_list_first_entry (&connection->shm_pools,
								 cairo_xcb_shm_mem_pool_t,
								 link));
    }
}

#endif /* CAIRO_HAS_XCB_SHM_FUNCTIONS */
