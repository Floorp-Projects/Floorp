/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Novell code.
 *
 * The Initial Developer of the Original Code is Novell.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   rocallahan@novell.com
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "cairo-gdk-utils.h"

#include <stdlib.h>

#if   HAVE_STDINT_H
#include <stdint.h>
#elif HAVE_INTTYPES_H
#include <inttypes.h>
#elif HAVE_SYS_INT_TYPES_H
#include <sys/int_types.h>
#endif


/* We have three basic strategies available:
   1) 'direct': cr targets a native surface, and other conditions are met: we can
      pass the underlying drawable directly to the callback
   2) 'opaque': the image is opaque: we can create a temporary cairo native surface,
      pass its underlying drawable to the callback, and paint the result
      using cairo
   3) 'default': create a temporary cairo native surface, fill with black, pass its
      underlying drawable to the callback, copy the results to a cairo
      image surface, repeat with a white background, update the on-black
      image alpha values by comparing the two images, then paint the on-black
      image using cairo
   Sure would be nice to have an X extension to do 3 for us on the server...
*/

static cairo_bool_t
_convert_coord_to_short (double coord, short *v)
{
    *v = (short)coord;
    /* XXX allow some tolerance here? */
    return *v == coord;
}

static cairo_bool_t
_convert_coord_to_unsigned_short (double coord, unsigned short *v)
{
    *v = (unsigned short)coord;
    /* XXX allow some tolerance here? */
    return *v == coord;
}


void cairo_draw_with_gdk (cairo_t *cr,
                           cairo_gdk_drawing_callback callback,
                           void * closure,
                           unsigned int width, unsigned int height,
                           cairo_gdk_drawing_opacity_t is_opaque,
                           cairo_gdk_drawing_support_t capabilities,
                           cairo_gdk_drawing_result_t *result)
{
    double device_offset_x, device_offset_y;
    short offset_x = 0, offset_y = 0;
    //cairo_surface_t * target = cairo_get_group_target (cr);
    cairo_surface_t * target = cairo_get_target (cr);
    cairo_matrix_t matrix;

    cairo_surface_get_device_offset (target, &device_offset_x, &device_offset_y);
    cairo_get_matrix (cr, &matrix);

    _convert_coord_to_short (matrix.x0 + device_offset_x, &offset_x);
    _convert_coord_to_short (matrix.y0 + device_offset_y, &offset_y);

    cairo_surface_flush (target);
    callback (closure, target, offset_x, offset_y, NULL, 0);
    cairo_surface_mark_dirty (target);
}


