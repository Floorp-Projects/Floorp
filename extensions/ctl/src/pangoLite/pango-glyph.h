/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * Pango
 * pango-glyph.h:
 *
 * The contents of this file are subject to the Mozilla Public	
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Pango Library (www.pango.org) 
 * 
 * The Initial Developer of the Original Code is Red Hat Software
 * Portions created by Red Hat are Copyright (C) 2000 Red Hat Software.
 * 
 * Contributor(s): 
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Lessor General Public License Version 2 (the 
 * "LGPL"), in which case the provisions of the LGPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the LGPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the LGPL. If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * LGPL.
*/

#ifndef __PANGO_GLYPH_H__
#define __PANGO_GLYPH_H__

#include "pango-types.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _PangoGlyphVisAttr PangoGlyphVisAttr;
typedef struct _PangoGlyphInfo PangoGlyphInfo;
typedef struct _PangoGlyphString PangoGlyphString;

/* 1000ths of a device unit */
typedef gint32 PangoGlyphUnit;

/* Visual attributes of a glyph
 */
struct _PangoGlyphVisAttr
{
  guint is_cluster_start : 1;
};

/* A single glyph 
 */
struct _PangoGlyphInfo
{
  PangoGlyph        glyph;
  PangoGlyphVisAttr attr;
};

/* A string of glyphs with positional information and visual attributes -
 * ready for drawing
 */
struct _PangoGlyphString {
  gint num_glyphs;

  PangoGlyphInfo *glyphs;

  /* This is a memory inefficient way of representing the information
   * here - each value gives the byte index within the text
   * corresponding to the glyph string of the start of the cluster to
   * which the glyph belongs.
   */
  gint *log_clusters;

  /*< private >*/
  gint space;
};

PangoGlyphString *pango_glyph_string_new(void);
void pango_glyph_string_set_size(PangoGlyphString *string, gint new_len);
void pango_glyph_string_free(PangoGlyphString *string);

void pango_glyph_string_index_to_x(PangoGlyphString *glyphs,
                                   char             *text,
                                   int              length,
                                   PangoAnalysis    *analysis,
                                   int              index,
                                   gboolean         trailing,
                                   int              *x_pos);
void pango_glyph_string_x_to_index(PangoGlyphString *glyphs,
                                   char             *text,
                                   int              length,
                                   PangoAnalysis    *analysis,
                                   int              x_pos,
                                   int              *index,
                                   int              *trailing);

/* Turn a string of characters into a string of glyphs */
void pango_shape(const char       *text,
                 gint             length,
                 PangoAnalysis    *analysis,
                 PangoGlyphString *glyphs);

GList *pango_reorder_items(GList *logical_items);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PANGO_GLYPH_H__ */
