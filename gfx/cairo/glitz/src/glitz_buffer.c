/*
 * Copyright © 2004 David Reveman
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * David Reveman not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * David Reveman makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * DAVID REVEMAN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL DAVID REVEMAN BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: David Reveman <davidr@novell.com>
 */

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include "glitzint.h"

static glitz_status_t
_glitz_buffer_init (glitz_buffer_t      *buffer,
		    glitz_drawable_t    *drawable,
		    void                *data,
		    unsigned int        size,
		    glitz_buffer_hint_t hint)
{
    glitz_gl_enum_t usage;

    buffer->ref_count = 1;
    buffer->name = 0;

    if (drawable)
    {
	GLITZ_GL_DRAWABLE (drawable);

	switch (hint) {
	case GLITZ_BUFFER_HINT_STREAM_DRAW:
	    usage = GLITZ_GL_STREAM_DRAW;
	    break;
	case GLITZ_BUFFER_HINT_STREAM_READ:
	    usage = GLITZ_GL_STREAM_READ;
	    break;
	case GLITZ_BUFFER_HINT_STREAM_COPY:
	    usage = GLITZ_GL_STREAM_COPY;
	    break;
	case GLITZ_BUFFER_HINT_STATIC_DRAW:
	    usage = GLITZ_GL_STATIC_DRAW;
	    break;
	case GLITZ_BUFFER_HINT_STATIC_READ:
	    usage = GLITZ_GL_STATIC_READ;
	    break;
	case GLITZ_BUFFER_HINT_STATIC_COPY:
	    usage = GLITZ_GL_STATIC_COPY;
	    break;
	case GLITZ_BUFFER_HINT_DYNAMIC_DRAW:
	    usage = GLITZ_GL_DYNAMIC_DRAW;
	    break;
	case GLITZ_BUFFER_HINT_DYNAMIC_READ:
	    usage = GLITZ_GL_DYNAMIC_READ;
	    break;
	default:
	    usage = GLITZ_GL_DYNAMIC_COPY;
	    break;
	}

	buffer->owns_data = 1;
	buffer->drawable = drawable;
	glitz_drawable_reference (drawable);

	drawable->backend->push_current (drawable, NULL,
					 GLITZ_ANY_CONTEXT_CURRENT);

	gl->gen_buffers (1, &buffer->name);
	if (buffer->name) {
	    gl->bind_buffer (buffer->target, buffer->name);
	    gl->buffer_data (buffer->target, size, data, usage);
	    gl->bind_buffer (buffer->target, 0);
	}

	drawable->backend->pop_current (drawable);
    }
    else
    {
	buffer->drawable = NULL;
	usage = GLITZ_GL_DYNAMIC_COPY;
    }

    if (size > 0 && buffer->name == 0)
    {
	buffer->data = malloc (size);
	if (buffer->data == NULL)
	    return GLITZ_STATUS_NO_MEMORY;

	if (data)
	    memcpy (buffer->data, data, size);

	buffer->owns_data = 1;
    }
    else
    {
	buffer->owns_data = 0;
	buffer->data = data;
    }

    return GLITZ_STATUS_SUCCESS;
}

glitz_buffer_t *
glitz_vertex_buffer_create (glitz_drawable_t    *drawable,
			    void                *data,
			    unsigned int        size,
			    glitz_buffer_hint_t hint)
{
    glitz_buffer_t *buffer;
    glitz_status_t status;

    if (size == 0)
	return NULL;

    buffer = (glitz_buffer_t *) malloc (sizeof (glitz_buffer_t));
    if (buffer == NULL)
	return NULL;

    buffer->target = GLITZ_GL_ARRAY_BUFFER;

    if (drawable->backend->feature_mask &
	GLITZ_FEATURE_VERTEX_BUFFER_OBJECT_MASK)
	status = _glitz_buffer_init (buffer, drawable, data, size, hint);
    else
	status = _glitz_buffer_init (buffer, NULL, data, size, hint);

    if (status != GLITZ_STATUS_SUCCESS) {
	free (buffer);
	return NULL;
    }

    return buffer;
}

glitz_buffer_t *
glitz_pixel_buffer_create (glitz_drawable_t    *drawable,
			   void                *data,
			   unsigned int        size,
			   glitz_buffer_hint_t hint)
{
    glitz_buffer_t *buffer;
    glitz_status_t status;

    if (size == 0)
	return NULL;

    buffer = (glitz_buffer_t *) malloc (sizeof (glitz_buffer_t));
    if (buffer == NULL)
	return NULL;

    switch (hint) {
    case GLITZ_BUFFER_HINT_STREAM_READ:
    case GLITZ_BUFFER_HINT_STATIC_READ:
    case GLITZ_BUFFER_HINT_DYNAMIC_READ:
	buffer->target = GLITZ_GL_PIXEL_PACK_BUFFER;
	break;
    default:
	buffer->target = GLITZ_GL_PIXEL_UNPACK_BUFFER;
	break;
    }

    if (drawable->backend->feature_mask &
        GLITZ_FEATURE_PIXEL_BUFFER_OBJECT_MASK)
	status = _glitz_buffer_init (buffer, drawable, data, size, hint);
    else
	status = _glitz_buffer_init (buffer, NULL, data, size, hint);

    if (status != GLITZ_STATUS_SUCCESS) {
	free (buffer);
	return NULL;
    }

    return buffer;
}

glitz_buffer_t *
glitz_buffer_create_for_data (void *data)
{
    glitz_buffer_t *buffer;

    buffer = (glitz_buffer_t *) malloc (sizeof (glitz_buffer_t));
    if (buffer == NULL)
	return NULL;

    if (_glitz_buffer_init (buffer, NULL, data, 0, 0)) {
	free (buffer);
	return NULL;
    }

    return buffer;
}

void
glitz_buffer_destroy (glitz_buffer_t *buffer)
{
    if (!buffer)
	return;

    buffer->ref_count--;
    if (buffer->ref_count)
	return;

    if (buffer->drawable) {
	buffer->drawable->backend->push_current (buffer->drawable, NULL,
						 GLITZ_ANY_CONTEXT_CURRENT);
	buffer->drawable->backend->gl->delete_buffers (1, &buffer->name);
	buffer->drawable->backend->pop_current (buffer->drawable);
	glitz_drawable_destroy (buffer->drawable);
    } else if (buffer->owns_data)
	free (buffer->data);

    free (buffer);
}

void
glitz_buffer_reference (glitz_buffer_t *buffer)
{
    if (!buffer)
	return;

    buffer->ref_count++;
}

void
glitz_buffer_set_data (glitz_buffer_t *buffer,
		       int            offset,
		       unsigned int   size,
		       const void     *data)
{
    if (buffer->drawable) {
	GLITZ_GL_DRAWABLE (buffer->drawable);

	buffer->drawable->backend->push_current (buffer->drawable, NULL,
						 GLITZ_ANY_CONTEXT_CURRENT);
	gl->bind_buffer (buffer->target, buffer->name);
	gl->buffer_sub_data (buffer->target, offset, size, data);
	gl->bind_buffer (buffer->target, 0);
	buffer->drawable->backend->pop_current (buffer->drawable);
    } else if (buffer->data)
	memcpy ((char *) buffer->data + offset, data, size);
}
slim_hidden_def(glitz_buffer_set_data);

void
glitz_buffer_get_data (glitz_buffer_t *buffer,
		       int            offset,
		       unsigned int   size,
		       void           *data)
{
    if (buffer->drawable) {
	GLITZ_GL_DRAWABLE (buffer->drawable);

	buffer->drawable->backend->push_current (buffer->drawable, NULL,
						 GLITZ_ANY_CONTEXT_CURRENT);

	gl->bind_buffer (buffer->target, buffer->name);
	gl->get_buffer_sub_data (buffer->target, offset, size, data);
	gl->bind_buffer (buffer->target, 0);

	buffer->drawable->backend->pop_current (buffer->drawable);
    } else if (buffer->data)
	memcpy (data, (char *) buffer->data + offset, size);
}
slim_hidden_def(glitz_buffer_get_data);

void *
glitz_buffer_map (glitz_buffer_t        *buffer,
		  glitz_buffer_access_t access)
{
    void *pointer = NULL;

    if (buffer->drawable) {
	glitz_gl_enum_t buffer_access;

	GLITZ_GL_DRAWABLE (buffer->drawable);

	buffer->drawable->backend->push_current (buffer->drawable, NULL,
						 GLITZ_ANY_CONTEXT_CURRENT);

	switch (access) {
	case GLITZ_BUFFER_ACCESS_READ_ONLY:
	    buffer_access = GLITZ_GL_READ_ONLY;
	    break;
	case GLITZ_BUFFER_ACCESS_WRITE_ONLY:
	    buffer_access = GLITZ_GL_WRITE_ONLY;
	    break;
	default:
	    buffer_access = GLITZ_GL_READ_WRITE;
	    break;
	}

	gl->bind_buffer (buffer->target, buffer->name);
	pointer = gl->map_buffer (buffer->target, buffer_access);
	gl->bind_buffer (buffer->target, 0);

	buffer->drawable->backend->pop_current (buffer->drawable);
    }

    if (pointer == NULL)
	pointer = buffer->data;

    return pointer;
}

glitz_status_t
glitz_buffer_unmap (glitz_buffer_t *buffer)
{
    glitz_status_t status = GLITZ_STATUS_SUCCESS;

    if (buffer->drawable) {
	GLITZ_GL_DRAWABLE (buffer->drawable);

	buffer->drawable->backend->push_current (buffer->drawable, NULL,
						 GLITZ_ANY_CONTEXT_CURRENT);

	gl->bind_buffer (buffer->target, buffer->name);

	if (gl->unmap_buffer (buffer->target) == GLITZ_GL_FALSE)
	    status = GLITZ_STATUS_CONTENT_DESTROYED;

	gl->bind_buffer (buffer->target, 0);

	buffer->drawable->backend->pop_current (buffer->drawable);
    }

    return status;
}

void *
glitz_buffer_bind (glitz_buffer_t *buffer,
		   glitz_gl_enum_t target)
{
    if (buffer->drawable) {
	buffer->drawable->backend->gl->bind_buffer (target, buffer->name);
	buffer->target = target;

	return NULL;
    }

    return buffer->data;
}

void
glitz_buffer_unbind (glitz_buffer_t *buffer)
{
    if (buffer->drawable)
	buffer->drawable->backend->gl->bind_buffer (buffer->target, 0);
}
