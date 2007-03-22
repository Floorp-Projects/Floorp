/*
 * Copyright © 2004 David Reveman, Peter Nilsson
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the names of
 * David Reveman and Peter Nilsson not be used in advertising or
 * publicity pertaining to distribution of the software without
 * specific, written prior permission. David Reveman and Peter Nilsson
 * makes no representations about the suitability of this software for
 * any purpose. It is provided "as is" without express or implied warranty.
 *
 * DAVID REVEMAN AND PETER NILSSON DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL DAVID REVEMAN AND
 * PETER NILSSON BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors: David Reveman <davidr@novell.com>
 *          Peter Nilsson <c99pnn@cs.umu.se>
 */

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include "glitzint.h"

unsigned long
glitz_status_to_status_mask (glitz_status_t status)
{
    switch (status) {
    case GLITZ_STATUS_NO_MEMORY:
	return GLITZ_STATUS_NO_MEMORY_MASK;
    case GLITZ_STATUS_BAD_COORDINATE:
	return GLITZ_STATUS_BAD_COORDINATE_MASK;
    case GLITZ_STATUS_NOT_SUPPORTED:
	return GLITZ_STATUS_NOT_SUPPORTED_MASK;
    case GLITZ_STATUS_CONTENT_DESTROYED:
	return GLITZ_STATUS_CONTENT_DESTROYED_MASK;
    case GLITZ_STATUS_SUCCESS:
	break;
    }

    return 0;
}

glitz_status_t
glitz_status_pop_from_mask (unsigned long *mask)
{
    if (*mask & GLITZ_STATUS_NO_MEMORY_MASK) {
	*mask &= ~GLITZ_STATUS_NO_MEMORY_MASK;
	return GLITZ_STATUS_NO_MEMORY;
    } else if (*mask & GLITZ_STATUS_BAD_COORDINATE_MASK) {
	*mask &= ~GLITZ_STATUS_BAD_COORDINATE_MASK;
	return GLITZ_STATUS_BAD_COORDINATE;
    } else if (*mask & GLITZ_STATUS_NOT_SUPPORTED_MASK) {
	*mask &= ~GLITZ_STATUS_NOT_SUPPORTED_MASK;
	return GLITZ_STATUS_NOT_SUPPORTED;
    } else if (*mask & GLITZ_STATUS_CONTENT_DESTROYED_MASK) {
	*mask &= ~GLITZ_STATUS_CONTENT_DESTROYED_MASK;
	return GLITZ_STATUS_CONTENT_DESTROYED;
    }

    return GLITZ_STATUS_SUCCESS;
}

const char *
glitz_status_string (glitz_status_t status)
{
    switch (status) {
    case GLITZ_STATUS_SUCCESS:
	return "success";
    case GLITZ_STATUS_NO_MEMORY:
	return "out of memory";
    case GLITZ_STATUS_BAD_COORDINATE:
	return "bad coordinate";
    case GLITZ_STATUS_NOT_SUPPORTED:
	return "not supported";
    case GLITZ_STATUS_CONTENT_DESTROYED:
	return "content destroyed";
    }

    return "<unknown error status>";
}
