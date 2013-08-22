/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Effects.h"
#include "LayersLogging.h"              // for AppendToString
#include "nsAString.h"
#include "nsPrintfCString.h"            // for nsPrintfCString
#include "nsString.h"                   // for nsAutoCString

using namespace mozilla::layers;

#ifdef MOZ_LAYERS_HAVE_LOG
void
TexturedEffect::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  aTo += aPrefix;
  aTo += nsPrintfCString("%s (0x%p)", Name(), this);
  AppendToString(aTo, mTextureCoords, " [texture-coords=", "]");

  if (mPremultiplied) {
    aTo += " [premultiplied]";
  } else {
    aTo += " [not-premultiplied]";
  }

  AppendToString(aTo, mFilter, " [filter=", "]");
}

void
EffectMask::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  aTo += aPrefix;
  aTo += nsPrintfCString("EffectMask (0x%p)", this);
  AppendToString(aTo, mSize, " [size=", "]");
  AppendToString(aTo, mMaskTransform, " [mask-transform=", "]");

  if (mIs3D) {
    aTo += " [is-3d]";
  }

  if (mMaskTexture) {
    nsAutoCString prefix(aPrefix);
    prefix += "  ";

    aTo += "\n";
    mMaskTexture->PrintInfo(aTo, prefix.get());
  }
}

void
EffectRenderTarget::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  TexturedEffect::PrintInfo(aTo, aPrefix);
  aTo += nsPrintfCString(" [render-target=%p]", mRenderTarget.get());
}

void
EffectSolidColor::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  aTo += aPrefix;
  aTo += nsPrintfCString("EffectSolidColor (0x%p) [color=%x]", this, mColor.ToABGR());
}

#endif // MOZ_LAYERS_HAVE_LOG
