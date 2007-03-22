/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * Pango
 * pango-types.h:
 *
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
 * The Original Code is the Pango Library (www.pango.org).
 *
 * The Initial Developer of the Original Code is
 * Red Hat Software.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef __PANGO_TYPES_H__
#define __PANGO_TYPES_H__

#include <glib.h>

typedef struct _PangoliteLangRange PangoliteLangRange;
typedef struct _PangoliteLogAttr PangoliteLogAttr;

typedef struct _PangoliteEngineShape PangoliteEngineShape;

/* A index of a glyph into a font. Rendering system dependent
 */
typedef guint32 PangoliteGlyph;

/* Information about a segment of text with a consistent
 * shaping/language engine and bidirectional level
 */
typedef enum {
  PANGO_DIRECTION_LTR,
  PANGO_DIRECTION_RTL,
  PANGO_DIRECTION_TTB_LTR,
  PANGO_DIRECTION_TTB_RTL
} PangoliteDirection;

/* Language tagging information
 */
struct _PangoliteLangRange
{
  gint  start;
  gint  length;
  gchar *lang;
};

/* Will be of more use when run information is stored */
typedef struct _PangoliteAnalysis PangoliteAnalysis;

struct _PangoliteAnalysis
{
  char             *fontCharset;
  PangoliteEngineShape *shape_engine;
  /*  guint8           level; */
  PangoliteDirection   aDir;
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
