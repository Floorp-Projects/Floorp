/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Benoit Girard <bgirard@mozilla.com>
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

#include "Layers.h"
#include "RenderTrace.h"

// If rendertrace is off let's no compile this code
#ifdef MOZ_RENDERTRACE


namespace mozilla {
namespace layers {

static int colorId = 0;

// This should be done in the printf but android's printf is buggy
const char* colors[] = {
    "00", "01", "02", "03", "04", "05", "06", "07", "08", "09",
    "10", "11", "12", "13", "14", "15", "16", "17", "18", "19"
    };

static gfx3DMatrix GetRootTransform(Layer *aLayer) {
  gfx3DMatrix layerTrans = aLayer->GetTransform().ProjectTo2D();
  if (aLayer->GetParent() != NULL) {
    return GetRootTransform(aLayer->GetParent()) * layerTrans;
  }
  return layerTrans;
}

void RenderTraceLayers(Layer *aLayer, const char *aColor, const gfx3DMatrix aRootTransform, bool aReset) {
  if (!aLayer)
    return;

  gfx3DMatrix trans = aRootTransform * aLayer->GetTransform().ProjectTo2D();
  nsIntRect clipRect = aLayer->GetEffectiveVisibleRegion().GetBounds();
  gfxRect rect(clipRect.x, clipRect.y, clipRect.width, clipRect.height);
  trans.TransformBounds(rect);

  printf_stderr("%s RENDERTRACE %u rect #%02X%s %i %i %i %i\n",
    aLayer->Name(), (int)PR_IntervalNow(),
    colorId, aColor,
    (int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height);

  colorId++;

  for (Layer* child = aLayer->GetFirstChild();
        child; child = child->GetNextSibling()) {
    RenderTraceLayers(child, aColor, aRootTransform, false);
  }

  if (aReset) colorId = 0;
}

void RenderTraceInvalidateStart(Layer *aLayer, const char *aColor, const nsIntRect aRect) {
  gfx3DMatrix trans = GetRootTransform(aLayer);
  gfxRect rect(aRect.x, aRect.y, aRect.width, aRect.height);
  trans.TransformBounds(rect);

  printf_stderr("%s RENDERTRACE %u fillrect #%s %i %i %i %i\n",
    aLayer->Name(), (int)PR_IntervalNow(),
    aColor,
    (int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height);
}
void RenderTraceInvalidateEnd(Layer *aLayer, const char *aColor) {
  // Clear with an empty rect
  RenderTraceInvalidateStart(aLayer, aColor, nsIntRect());
}

}
}

#endif

