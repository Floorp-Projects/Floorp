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
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Benoit Girard <bgirard@mozilla.com>
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

#include "LayerManagerD3D9.h"
#include "ShadowBufferD3D9.h"

#include "gfxWindowsSurface.h"
#include "gfxWindowsPlatform.h"


namespace mozilla {
namespace layers {

void 
ShadowBufferD3D9::Upload(gfxASurface* aUpdate, 
                         const nsIntRect& aVisibleRect)
{

  gfxIntSize size = aUpdate->GetSize();

  if (GetSize() != nsIntSize(size.width, size.height)) {
    HRESULT hr = mLayer->device()->CreateTexture(size.width, size.height, 1,
                                                 D3DUSAGE_DYNAMIC,
                                                 D3DFMT_A8R8G8B8,
                                                 D3DPOOL_DEFAULT, getter_AddRefs(mTexture), NULL);
    if (FAILED(hr)) {
      mLayer->ReportFailure(NS_LITERAL_CSTRING("ShadowBufferD3D9::Upload(): Failed to create texture"),
                            hr);
      return;
    }

    mTextureRect = aVisibleRect;
  }

  LockTextureRectD3D9 textureLock(mTexture);
  if (!textureLock.HasLock()) {
    NS_WARNING("Failed to lock ShadowBufferD3D9 texture.");
    return;
  }

  D3DLOCKED_RECT r = textureLock.GetLockRect();

  nsRefPtr<gfxImageSurface> imgSurface =
    new gfxImageSurface((unsigned char *)r.pBits,
                        GetSize(),
                        r.Pitch,
                        gfxASurface::ImageFormatARGB32);

  nsRefPtr<gfxContext> context = new gfxContext(imgSurface);
  context->SetSource(aUpdate);
  context->SetOperator(gfxContext::OPERATOR_SOURCE);
  context->Paint();

  imgSurface = NULL;
}

void 
ShadowBufferD3D9::RenderTo(LayerManagerD3D9 *aD3DManager, 
                           const nsIntRegion& aVisibleRegion)
{
  mLayer->SetShaderTransformAndOpacity();

  aD3DManager->SetShaderMode(DeviceManagerD3D9::RGBALAYER);
  mLayer->device()->SetTexture(0, mTexture);

  nsIntRegionRectIterator iter(aVisibleRegion);

  const nsIntRect *iterRect;
  while ((iterRect = iter.Next())) {
    mLayer->device()->SetVertexShaderConstantF(CBvLayerQuad,
                                       ShaderConstantRect(iterRect->x,
                                                          iterRect->y,
                                                          iterRect->width,
                                                          iterRect->height),
                                       1);

    mLayer->device()->SetVertexShaderConstantF(CBvTextureCoords,
      ShaderConstantRect(
        (float)(iterRect->x - mTextureRect.x) / (float)mTextureRect.width,
        (float)(iterRect->y - mTextureRect.y) / (float)mTextureRect.height,
        (float)iterRect->width / (float)mTextureRect.width,
        (float)iterRect->height / (float)mTextureRect.height), 1);

    mLayer->device()->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
  }
}

} /* namespace layers */
} /* namespace mozilla */
