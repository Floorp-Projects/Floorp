/*
 * Copyright © 2005 Novell, Inc.
 * 
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Novell, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 * Novell, Inc. makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without express or
 * implied warranty.
 *
 * NOVELL, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL NOVELL, INC. BE LIABLE FOR ANY SPECIAL, INDIRECT OR
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

void
glitz_framebuffer_init (glitz_framebuffer_t *framebuffer)
{
    framebuffer->name = 0;
}

void 
glitz_framebuffer_fini (glitz_gl_proc_address_list_t *gl,
                        glitz_framebuffer_t          *framebuffer)
{
    if (framebuffer->name)
        gl->delete_framebuffers (1, &framebuffer->name);
}

void
glitz_framebuffer_unbind (glitz_gl_proc_address_list_t *gl)
{
    gl->bind_framebuffer (GLITZ_GL_FRAMEBUFFER, 0);
}

glitz_bool_t
glitz_framebuffer_complete (glitz_gl_proc_address_list_t *gl,
                            glitz_framebuffer_t          *framebuffer,
                            glitz_texture_t              *texture)
{   
    if (!framebuffer->name)
    {
        if (!TEXTURE_ALLOCATED (texture))
            glitz_texture_allocate (gl, texture);
        
        gl->gen_framebuffers (1, &framebuffer->name);

        gl->bind_framebuffer (GLITZ_GL_FRAMEBUFFER, framebuffer->name);
    
        gl->framebuffer_texture_2d (GLITZ_GL_FRAMEBUFFER,
                                    GLITZ_GL_COLOR_ATTACHMENT0,
                                    GLITZ_GL_TEXTURE_2D, texture->name,
                                    0);
    }
    else
        gl->bind_framebuffer (GLITZ_GL_FRAMEBUFFER, framebuffer->name);

    return (gl->check_framebuffer_status (GLITZ_GL_FRAMEBUFFER) ==
            GLITZ_GL_FRAMEBUFFER_COMPLETE);
}
