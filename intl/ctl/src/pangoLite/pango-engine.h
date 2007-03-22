/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * Pango
 * pango-engine.h: Module handling
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
 * Portions created by the Initial Developer are Copyright (C) 2000
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

#ifndef __PANGO_ENGINE_H__
#define __PANGO_ENGINE_H__

#include "pango-types.h"
#include "pango-glyph.h"
#include "pango-coverage.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Module API */

#define PANGO_ENGINE_TYPE_SHAPE "PangoliteEngineShape"
#define PANGO_RENDER_TYPE_X    "PangoliteRenderX"
#define PANGO_RENDER_TYPE_NONE "PangoliteRenderNone"

typedef struct _PangoliteEngineInfo PangoliteEngineInfo;
typedef struct _PangoliteEngineRange PangoliteEngineRange;
typedef struct _PangoliteEngine PangoliteEngine;

struct _PangoliteEngineRange 
{
  guint32 start;
  guint32 end;
  gchar   *langs;
};

struct _PangoliteEngineInfo
{
  gchar            *id;
  gchar            *engine_type;
  gchar            *render_type;
  PangoliteEngineRange *ranges;
  gint             n_ranges;
};

struct _PangoliteEngine
{
  gchar *id;
  gchar *type;
  gint  length;
};

struct _PangoliteEngineShape
{
  PangoliteEngine engine;
  void (*script_shape) (const char       *fontCharset, 
                        const gunichar2  *text, 
                        int              length, 
                        PangoliteAnalysis    *analysis, 
                        PangoliteGlyphString *glyphs);
  PangoliteCoverage *(*get_coverage) (const char *fontCharset, const char *lang);

};

/* A module should export the following functions */
void         script_engine_list(PangoliteEngineInfo **engines, int *n_engines);
PangoliteEngine *script_engine_load(const char *id);
void         script_engine_unload(PangoliteEngine *engine);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PANGO_ENGINE_H__ */
