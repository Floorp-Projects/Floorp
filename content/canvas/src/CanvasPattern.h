/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CanvasPattern_h
#define mozilla_dom_CanvasPattern_h

#include "nsIDOMCanvasRenderingContext2D.h"

#define NS_CANVASPATTERNAZURE_PRIVATE_IID \
    {0xc9bacc25, 0x28da, 0x421e, {0x9a, 0x4b, 0xbb, 0xd6, 0x93, 0x05, 0x12, 0xbc}}

namespace mozilla {
namespace dom {

class CanvasPattern MOZ_FINAL : public nsIDOMCanvasPattern
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_CANVASPATTERNAZURE_PRIVATE_IID)

  enum RepeatMode
  {
    REPEAT,
    REPEATX,
    REPEATY,
    NOREPEAT
  };

  CanvasPattern(mozilla::gfx::SourceSurface* aSurface,
                RepeatMode aRepeat,
                nsIPrincipal* principalForSecurityCheck,
                bool forceWriteOnly,
                bool CORSUsed)
    : mSurface(aSurface)
    , mRepeat(aRepeat)
    , mPrincipal(principalForSecurityCheck)
    , mForceWriteOnly(forceWriteOnly)
    , mCORSUsed(CORSUsed)
  {
  }

  NS_DECL_ISUPPORTS

  mozilla::RefPtr<mozilla::gfx::SourceSurface> mSurface;
  const RepeatMode mRepeat;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  const bool mForceWriteOnly;
  const bool mCORSUsed;
};

}
}

#endif // mozilla_dom_CanvasPattern_h
