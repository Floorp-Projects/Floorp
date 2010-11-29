/*
 * Copyright (C) 2009  Red Hat, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Red Hat Author(s): Behdad Esfahbod
 */

#include "hb-private.h"

#include "hb-shape.h"

#include "hb-buffer-private.hh"

#include "hb-ot-shape.h"

#ifdef HAVE_GRAPHITE
#include "hb-graphite.h"
#endif

HB_BEGIN_DECLS


void
hb_shape (hb_font_t    *font,
	  hb_face_t    *face,
	  hb_buffer_t  *buffer,
	  hb_feature_t *features,
	  unsigned int  num_features)
{
#if 0 && defined(HAVE_GRAPHITE)
  hb_blob_t *silf_blob;
  silf_blob = hb_face_get_table (face, HB_GRAPHITE_TAG_Silf);
  if (hb_blob_get_length(silf_blob))
  {
    hb_graphite_shape(font, face, buffer, features, num_features);
    hb_blob_destroy(silf_blob);
    return;
  }
  hb_blob_destroy(silf_blob);
#endif

  hb_ot_shape (font, face, buffer, features, num_features);
}


HB_END_DECLS
