/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * Pango
 * shape.c: Convert characters into glyphs.
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
 * Portions created by Red Hat are Copyright (C) 1999 Red Hat Software.
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

#include "pango-glyph.h"
#include "pango-engine.h"

/**
 * pangolite_shape:
 * @text:      the text to process
 * @length:    the length (in bytes) of @text
 * @analysis:  #PangoliteAnalysis structure from PangoliteItemize
 * @glyphs:    glyph string in which to store results
 *
 * Given a segment of text and the corresponding 
 * #PangoliteAnalysis structure returned from pangolite_itemize(),
 * convert the characters into glyphs. You may also pass
 * in only a substring of the item from pangolite_itemize().
 */
void pangolite_shape(const gunichar2  *text, 
                 gint             length, 
                 PangoliteAnalysis    *analysis,
                 PangoliteGlyphString *glyphs)
{
  if (analysis->shape_engine)
    analysis->shape_engine->script_shape(analysis->fontCharset, text, length, 
                                         analysis, glyphs);
  else {
    pangolite_glyph_string_set_size (glyphs, 1);
    
    glyphs->glyphs[0].glyph = 0;
    glyphs->log_clusters[0] = 0;
  }
  
  g_assert (glyphs->num_glyphs > 0);
}
