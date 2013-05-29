/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CanvasPattern_h
#define mozilla_dom_CanvasPattern_h

#include "mozilla/dom/CanvasRenderingContext2DBinding.h"
#include "mozilla/dom/CanvasRenderingContext2D.h"
#include "mozilla/RefPtr.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

#define NS_CANVASPATTERNAZURE_PRIVATE_IID \
    {0xc9bacc25, 0x28da, 0x421e, {0x9a, 0x4b, 0xbb, 0xd6, 0x93, 0x05, 0x12, 0xbc}}
class nsIPrincipal;

namespace mozilla {
namespace gfx {
class SourceSurface;
}

namespace dom {

class CanvasPattern MOZ_FINAL : public nsISupports,
                                public nsWrapperCache
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_CANVASPATTERNAZURE_PRIVATE_IID)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(CanvasPattern)

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

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
  {
    return CanvasPatternBinding::Wrap(aCx, aScope, this);
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
