/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CanvasPattern_h
#define mozilla_dom_CanvasPattern_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/CanvasRenderingContext2DBinding.h"
#include "mozilla/dom/CanvasRenderingContext2D.h"
#include "mozilla/RefPtr.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

class nsIPrincipal;

namespace mozilla {
namespace gfx {
class SourceSurface;
}

namespace dom {

class CanvasPattern MOZ_FINAL : public nsWrapperCache
{
public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(CanvasPattern)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(CanvasPattern)

  enum RepeatMode
  {
    REPEAT,
    REPEATX,
    REPEATY,
    NOREPEAT
  };

  CanvasPattern(CanvasRenderingContext2D* aContext,
                gfx::SourceSurface* aSurface,
                RepeatMode aRepeat,
                nsIPrincipal* principalForSecurityCheck,
                bool forceWriteOnly,
                bool CORSUsed)
    : mContext(aContext)
    , mSurface(aSurface)
    , mRepeat(aRepeat)
    , mPrincipal(principalForSecurityCheck)
    , mForceWriteOnly(forceWriteOnly)
    , mCORSUsed(CORSUsed)
  {
    SetIsDOMBinding();
  }

  JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE
  {
    return CanvasPatternBinding::Wrap(aCx, this);
  }

  CanvasRenderingContext2D* GetParentObject()
  {
    return mContext;
  }

  nsRefPtr<CanvasRenderingContext2D> mContext;
  RefPtr<gfx::SourceSurface> mSurface;
  const RepeatMode mRepeat;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  const bool mForceWriteOnly;
  const bool mCORSUsed;
};

}
}

#endif // mozilla_dom_CanvasPattern_h
