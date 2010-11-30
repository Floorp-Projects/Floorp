/*
 * Copyright (C) 2010  Google, Inc.
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
 * Google Author(s): Behdad Esfahbod
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "hb.h"

#ifdef HAVE_GLIB
#include <glib.h>
#endif
#include <stdlib.h>
#include <stdio.h>

HB_BEGIN_DECLS


int
main (int argc, char **argv)
{
  hb_blob_t *blob = NULL;
  hb_face_t *face = NULL;

  if (argc != 2) {
    fprintf (stderr, "usage: %s font-file.ttf\n", argv[0]);
    exit (1);
  }

  /* Create the blob */
  {
    const char *font_data;
    unsigned int len;
    hb_destroy_func_t destroy;
    void *user_data;

#ifdef HAVE_GLIB
    GMappedFile *mf = g_mapped_file_new (argv[1], FALSE, NULL);
    font_data = g_mapped_file_get_contents (mf);
    len = g_mapped_file_get_length (mf);
    destroy = (hb_destroy_func_t) g_mapped_file_unref;
    user_data = (void *) mf;
#else
    FILE *f = fopen (argv[1], "rb");
    fseek (f, 0, SEEK_END);
    len = ftell (f);
    fseek (f, 0, SEEK_SET);
    font_data = (const char *) malloc (len);
    if (!font_data) len = 0;
    len = fread ((char *) font_data, 1, len, f);
    destroy = free;
    user_data = (void *) font_data;
    fclose (f);
#endif

    blob = hb_blob_create (font_data, len,
			   HB_MEMORY_MODE_READONLY_MAY_MAKE_WRITABLE,
			   destroy, user_data);
  }

  /* Create the face */
  face = hb_face_create_for_data (blob, 0 /* first face */);

  /* So, what now? */

  hb_face_destroy (face);
  hb_blob_destroy (blob);

  return 0;
}
