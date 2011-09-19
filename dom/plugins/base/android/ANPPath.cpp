/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Android NPAPI support code
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Doug Turner <dougt@mozilla.com>
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

#include "assert.h"
#include "ANPBase.h"
#include <android/log.h>

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoPlugins" , ## args)
#define ASSIGN(obj, name)   (obj)->name = anp_path_##name


// maybe this should store a list of actions (lineTo,
// moveTo), and when canvas_drawPath() we apply all of these
// actions to the gfxContext.

ANPPath*
anp_path_newPath()
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);
  return 0;
}


void
anp_path_deletePath(ANPPath* p)
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);
}


void
anp_path_copy(ANPPath* dst, const ANPPath* src)
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);
}


bool
anp_path_equal(const ANPPath* path0, const ANPPath* path1)
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);
  return false;
}


void
anp_path_reset(ANPPath* p)
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);
}


bool
anp_path_isEmpty(const ANPPath* p)
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);
  return false;
}


void
anp_path_getBounds(const ANPPath* p, ANPRectF* bounds)
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);

  bounds->left = 0;
  bounds->top = 0;
  bounds->right = 1000;
  bounds->left = 1000;
}


void
anp_path_moveTo(ANPPath* p, float x, float y)
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);
}

void
anp_path_lineTo(ANPPath* p, float x, float y)
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);
}

void
anp_path_quadTo(ANPPath* p, float x0, float y0, float x1, float y1)
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);
}

void
anp_path_cubicTo(ANPPath* p, float x0, float y0, float x1, float y1,
                      float x2, float y2)
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);
}

void
anp_path_close(ANPPath* p)
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);
}


void
anp_path_offset(ANPPath* src, float dx, float dy, ANPPath* dst)
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);
}


void
anp_path_transform(ANPPath* src, const ANPMatrix*, ANPPath* dst)
{
  LOG("%s - NOT IMPL.", __PRETTY_FUNCTION__);
}



void InitPathInterface(ANPPathInterfaceV0 *i) {
  _assert(i->inSize == sizeof(*i));
  ASSIGN(i, newPath);
  ASSIGN(i, deletePath);
  ASSIGN(i, copy);
  ASSIGN(i, equal);
  ASSIGN(i, reset);
  ASSIGN(i, isEmpty);
  ASSIGN(i, getBounds);
  ASSIGN(i, moveTo);
  ASSIGN(i, lineTo);
  ASSIGN(i, quadTo);
  ASSIGN(i, cubicTo);
  ASSIGN(i, close);
  ASSIGN(i, offset);
  ASSIGN(i, transform);
}
