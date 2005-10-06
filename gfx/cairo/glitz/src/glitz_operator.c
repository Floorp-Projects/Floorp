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

void
glitz_set_operator (glitz_gl_proc_address_list_t *gl,
		    glitz_operator_t		 op)
{
    switch (op) {
    case GLITZ_OPERATOR_CLEAR:
	gl->enable (GLITZ_GL_BLEND);
	gl->blend_func (GLITZ_GL_ZERO, GLITZ_GL_ZERO);
	break;
    case GLITZ_OPERATOR_SRC:
	gl->disable (GLITZ_GL_BLEND);
	break;
    case GLITZ_OPERATOR_DST:
	gl->enable (GLITZ_GL_BLEND);
	gl->blend_func (GLITZ_GL_ZERO, GLITZ_GL_ONE);
	break;
    case GLITZ_OPERATOR_OVER:
	gl->enable (GLITZ_GL_BLEND);
	gl->blend_func (GLITZ_GL_ONE, GLITZ_GL_ONE_MINUS_SRC_ALPHA);
	break;
    case GLITZ_OPERATOR_OVER_REVERSE:
	gl->enable (GLITZ_GL_BLEND);
	gl->blend_func (GLITZ_GL_ONE_MINUS_DST_ALPHA, GLITZ_GL_ONE);
	break;
    case GLITZ_OPERATOR_IN:
	gl->enable (GLITZ_GL_BLEND);
	gl->blend_func (GLITZ_GL_DST_ALPHA, GLITZ_GL_ZERO);
	break;
    case GLITZ_OPERATOR_IN_REVERSE:
	gl->enable (GLITZ_GL_BLEND);
	gl->blend_func (GLITZ_GL_ZERO, GLITZ_GL_SRC_ALPHA);
	break;
    case GLITZ_OPERATOR_OUT:
	gl->enable (GLITZ_GL_BLEND);
	gl->blend_func (GLITZ_GL_ONE_MINUS_DST_ALPHA, GLITZ_GL_ZERO);
	break;
    case GLITZ_OPERATOR_OUT_REVERSE:
	gl->enable (GLITZ_GL_BLEND);
	gl->blend_func (GLITZ_GL_ZERO, GLITZ_GL_ONE_MINUS_SRC_ALPHA);
	break;
    case GLITZ_OPERATOR_ATOP:
	gl->enable (GLITZ_GL_BLEND);
	gl->blend_func (GLITZ_GL_DST_ALPHA, GLITZ_GL_ONE_MINUS_SRC_ALPHA);
	break;
    case GLITZ_OPERATOR_ATOP_REVERSE:
	gl->enable (GLITZ_GL_BLEND);
	gl->blend_func (GLITZ_GL_ONE_MINUS_DST_ALPHA, GLITZ_GL_SRC_ALPHA);
	break;
    case GLITZ_OPERATOR_XOR:
	gl->enable (GLITZ_GL_BLEND);
	gl->blend_func (GLITZ_GL_ONE_MINUS_DST_ALPHA,
			GLITZ_GL_ONE_MINUS_SRC_ALPHA);
	break;
    case GLITZ_OPERATOR_ADD:
	gl->enable (GLITZ_GL_BLEND);
	gl->blend_func (GLITZ_GL_ONE, GLITZ_GL_ONE);
	break;
    }
}
