/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TextMetrics_h
#define mozilla_dom_TextMetrics_h

#include "mozilla/dom/CanvasRenderingContext2DBinding.h"
#include "mozilla/dom/NonRefcountedDOMObject.h"

namespace mozilla {
namespace dom {

class TextMetrics MOZ_FINAL : public NonRefcountedDOMObject
{
public:
  TextMetrics(float w) : width(w)
  {
    MOZ_COUNT_CTOR(TextMetrics);
  }

  ~TextMetrics()
  {
    MOZ_COUNT_DTOR(TextMetrics);
  }

  float Width() const
  {
    return width;
  }

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope,
                       bool* aTookOwnership)
  {
    return TextMetricsBinding::Wrap(aCx, aScope, this, aTookOwnership);
  }

private:
  float width;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TextMetrics_h
