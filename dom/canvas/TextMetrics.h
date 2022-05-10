/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TextMetrics_h
#define mozilla_dom_TextMetrics_h

#include "mozilla/dom/CanvasRenderingContext2DBinding.h"
#include "mozilla/dom/NonRefcountedDOMObject.h"

namespace mozilla::dom {

class TextMetrics final : public NonRefcountedDOMObject {
 public:
  explicit TextMetrics(double aWidth, double aActualBoundingBoxLeft,
                       double aActualBoundingBoxRight,
                       double aFontBoundingBoxAscent,
                       double aFontBoundingBoxDescent,
                       double aActualBoundingBoxAscent,
                       double aActualBoundingBoxDescent, double aEmHeightAscent,
                       double aEmHeightDescent, double aHangingBaseline,
                       double aAlphabeticBaseline, double aIdeographicBaseline)
      : width(aWidth),
        actualBoundingBoxLeft(aActualBoundingBoxLeft),
        actualBoundingBoxRight(aActualBoundingBoxRight),
        fontBoundingBoxAscent(aFontBoundingBoxAscent),
        fontBoundingBoxDescent(aFontBoundingBoxDescent),
        actualBoundingBoxAscent(aActualBoundingBoxAscent),
        actualBoundingBoxDescent(aActualBoundingBoxDescent),
        emHeightAscent(aEmHeightAscent),
        emHeightDescent(aEmHeightDescent),
        hangingBaseline(aHangingBaseline),
        alphabeticBaseline(aAlphabeticBaseline),
        ideographicBaseline(aIdeographicBaseline) {
    MOZ_COUNT_CTOR(TextMetrics);
  }

  MOZ_COUNTED_DTOR(TextMetrics)

  double Width() const { return width; }

  double ActualBoundingBoxLeft() const { return actualBoundingBoxLeft; }

  double ActualBoundingBoxRight() const { return actualBoundingBoxRight; }

  // y-direction

  double FontBoundingBoxAscent() const { return fontBoundingBoxAscent; }

  double FontBoundingBoxDescent() const { return fontBoundingBoxDescent; }

  double ActualBoundingBoxAscent() const { return actualBoundingBoxAscent; }

  double ActualBoundingBoxDescent() const { return actualBoundingBoxDescent; }

  double EmHeightAscent() const { return emHeightAscent; }

  double EmHeightDescent() const { return emHeightDescent; }

  double HangingBaseline() const { return hangingBaseline; }

  double AlphabeticBaseline() const { return alphabeticBaseline; }

  double IdeographicBaseline() const { return ideographicBaseline; }

  bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
                  JS::MutableHandle<JSObject*> aReflector) {
    return TextMetrics_Binding::Wrap(aCx, this, aGivenProto, aReflector);
  }

 private:
  double width;
  double actualBoundingBoxLeft;
  double actualBoundingBoxRight;
  double fontBoundingBoxAscent;
  double fontBoundingBoxDescent;
  double actualBoundingBoxAscent;
  double actualBoundingBoxDescent;
  double emHeightAscent;
  double emHeightDescent;
  double hangingBaseline;
  double alphabeticBaseline;
  double ideographicBaseline;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_TextMetrics_h
