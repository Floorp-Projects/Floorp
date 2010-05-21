/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert O'Callahan <robert@ocallahan.org>
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

#include "ColorLayerOGL.h"

namespace mozilla {
namespace layers {

LayerOGL::LayerType
ColorLayerOGL::GetType()
{
  return TYPE_COLOR;
}

Layer*
ColorLayerOGL::GetLayer()
{
  return this;
}

void
ColorLayerOGL::RenderLayer(int, DrawThebesLayerCallback, void*)
{
  static_cast<LayerManagerOGL*>(mManager)->MakeCurrent();

  // XXX we might be able to improve performance by using glClear

  float quadTransform[4][4];
  nsIntRect visibleRect = mVisibleRegion.GetBounds();
  // Transform the quad to the size of the visible area.
  memset(&quadTransform, 0, sizeof(quadTransform));
  quadTransform[0][0] = (float)visibleRect.width;
  quadTransform[1][1] = (float)visibleRect.height;
  quadTransform[2][2] = 1.0f;
  quadTransform[3][0] = (float)visibleRect.x;
  quadTransform[3][1] = (float)visibleRect.y;
  quadTransform[3][3] = 1.0f;
  
  ColorLayerProgram *program =
    static_cast<LayerManagerOGL*>(mManager)->GetColorLayerProgram();

  program->Activate();

  program->SetLayerQuadTransform(&quadTransform[0][0]);

  gfxRGBA color = mColor;
  // color is premultiplied, so we need to adjust all channels
  color.r *= GetOpacity();
  color.g *= GetOpacity();
  color.b *= GetOpacity();
  color.a *= GetOpacity();
  program->SetLayerColor(color);
  program->SetLayerTransform(&mTransform._11);
  program->Apply();

  gl()->fDrawArrays(LOCAL_GL_TRIANGLE_STRIP, 0, 4);
}

} /* layers */
} /* mozilla */
