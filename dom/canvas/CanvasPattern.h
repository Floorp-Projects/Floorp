/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CanvasPattern_h
#define mozilla_dom_CanvasPattern_h

#include "mozilla/dom/CanvasRenderingContext2DBinding.h"
#include "mozilla/RefPtr.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

class nsIPrincipal;

namespace mozilla {
namespace gfx {
class SourceSurface;
}  // namespace gfx

namespace dom {
class CanvasRenderingContext2D;
struct DOMMatrix2DInit;

class CanvasPattern final : public nsWrapperCache {
  ~CanvasPattern();

 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(CanvasPattern)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(CanvasPattern)

  enum class RepeatMode : uint8_t { REPEAT, REPEATX, REPEATY, NOREPEAT };

  CanvasPattern(CanvasRenderingContext2D* aContext,
                gfx::SourceSurface* aSurface, RepeatMode aRepeat,
                nsIPrincipal* principalForSecurityCheck, bool forceWriteOnly,
                bool CORSUsed);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override {
    return CanvasPattern_Binding::Wrap(aCx, this, aGivenProto);
  }

  CanvasRenderingContext2D* GetParentObject() { return mContext; }

  // WebIDL
  void SetTransform(const DOMMatrix2DInit& aInit, ErrorResult& aError);

  RefPtr<CanvasRenderingContext2D> mContext;
  RefPtr<gfx::SourceSurface> mSurface;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  mozilla::gfx::Matrix mTransform;
  const bool mForceWriteOnly;
  const bool mCORSUsed;
  const RepeatMode mRepeat;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_CanvasPattern_h
