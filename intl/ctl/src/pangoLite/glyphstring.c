/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * Pango
 * glyphstring.c:
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

#include <glib.h>
#include "pango-glyph.h"

/**
 * pangolite_glyph_string_new:
 *
 * Create a new PangoliteGlyphString.
 *
 * Returns the new PangoliteGlyphString
 */
PangoliteGlyphString *
pangolite_glyph_string_new(void)
{
  PangoliteGlyphString *string = g_new(PangoliteGlyphString, 1);
  
  string->num_glyphs = 0;
  string->space = 0;
  string->glyphs = NULL;
  string->log_clusters = NULL;
  return string;
}

/**
 * pangolite_glyph_string_set_size:
 * @string:    a PangoliteGlyphString.
 * @new_len:   the new length of the string.
 *
 * Resize a glyph string to the given length.
 */
void
pangolite_glyph_string_set_size(PangoliteGlyphString *string, gint new_len)
{
  g_return_if_fail (new_len >= 0);

  while (new_len > string->space) {
    if (string->space == 0)
      string->space = 1;
    else
      string->space *= 2;
    
    if (string->space < 0)
      g_error("%s: glyph string length overflows maximum integer size", 
              "pangolite_glyph_string_set_size");
  }
  
  string->glyphs = g_realloc(string->glyphs, 
                             string->space * sizeof(PangoliteGlyphInfo));
  string->log_clusters = g_realloc(string->log_clusters, 
                                   string->space * sizeof (gint));
  string->num_glyphs = new_len;
}

/**
 * pangolite_glyph_string_free:
 * @string:    a PangoliteGlyphString.
 *
 *  Free a glyph string and associated storage.
 */
void
pangolite_glyph_string_free(PangoliteGlyphString *string)
{
  g_free(string->glyphs);
  g_free(string->log_clusters);
  g_free(string);
}
