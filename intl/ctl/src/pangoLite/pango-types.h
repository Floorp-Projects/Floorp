/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * Pango
 * pango-types.h:
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

#ifndef __PANGO_TYPES_H__
#define __PANGO_TYPES_H__

#include <glib.h>

typedef struct _PangoLangRange PangoLangRange;
typedef struct _PangoLogAttr PangoLogAttr;

typedef struct _PangoEngineShape PangoEngineShape;

/* A index of a glyph into a font. Rendering system dependent
 */
typedef guint32 PangoGlyph;

/* Information about a segment of text with a consistent
 * shaping/language engine and bidirectional level
 */
typedef enum {
  PANGO_DIRECTION_LTR,
  PANGO_DIRECTION_RTL,
  PANGO_DIRECTION_TTB_LTR,
  PANGO_DIRECTION_TTB_RTL
} PangoDirection;

/* Language tagging information
 */
struct _PangoLangRange
{
  gint  start;
  gint  length;
  gchar *lang;
};

/* Will be of more use when run information is stored */
typedef struct _PangoAnalysis PangoAnalysis;

struct _PangoAnalysis
{
  char             *fontCharset;
  PangoEngineShape *shape_engine;
  /*  guint8           level; */
  PangoDirection   aDir;
};

#ifndef MOZ_WIDGET_GTK2 
/*glib2 already defined following 2 types*/
typedef guint32 gunichar;
typedef guint16 gunichar2;
#endif

#define G_CONST_RETURN const

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PANGO_TYPES_H__ */
