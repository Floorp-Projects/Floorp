/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CanvasGradient_h
#define mozilla_dom_CanvasGradient_h

#include "mozilla/Attributes.h"
#include "nsTArray.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/CanvasRenderingContext2DBinding.h"
#include "mozilla/dom/CanvasRenderingContext2D.h"
#include "mozilla/gfx/2D.h"
#include "nsWrapperCache.h"
#include "gfxGradientCache.h"

namespace mozilla {
namespace dom {

class CanvasGradient : public nsWrapperCache
{
public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(CanvasGradient)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(CanvasGradient)

  MOZ_BEGIN_NESTED_ENUM_CLASS(Type, uint8_t)
    LINEAR = 0,
    RADIAL
  MOZ_END_NESTED_ENUM_CLASS(Type)

  Type GetType()
  {
    return mType;
  }

  mozilla::gfx::GradientStops *
  GetGradientStopsForTarget(mozilla::gfx::DrawTarget *aRT)
  {
    if (mStops && mStops->GetBackendType() == aRT->GetType()) {
      return mStops;
    }

    mStops =
      gfx::gfxGradientCache::GetOrCreateGradientStops(aRT,
                                                      mRawStops,
                                                      gfx::ExtendMode::CLAMP);

    return mStops;
  }

  // WebIDL
  void AddColorStop(float offset, const nsAString& colorstr, ErrorResult& rv);

  JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE
  {
    return CanvasGradientBinding::Wrap(aCx, this);
  }

  CanvasRenderingContext2D* GetParentObject()
  {
    return mContext;
  }

protected:
  CanvasGradient(CanvasRenderingContext2D* aContext, Type aType)
    : mContext(aContext)
    , mType(aType)
  {
    SetIsDOMBinding();
  }

  nsRefPtr<CanvasRenderingContext2D> mContext;
  nsTArray<mozilla::gfx::GradientStop> mRawStops;
  mozilla::RefPtr<mozilla::gfx::GradientStops> mStops;
  Type mType;
  virtual ~CanvasGradient() {}
};

MOZ_FINISH_NESTED_ENUM_CLASS(CanvasGradient::Type)

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CanvasGradient_h
