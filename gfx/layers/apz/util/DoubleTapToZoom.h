/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_DoubleTapToZoom_h
#define mozilla_layers_DoubleTapToZoom_h

#include "Units.h"

template <class T>
class RefPtr;

namespace mozilla {
namespace dom {
class Document;
}

namespace layers {

enum class CantZoomOutBehavior : int8_t { Nothing = 0, ZoomIn };

struct ZoomTarget {
  // The preferred target rect that we'd like to zoom in on, if possible. An
  // empty rect means the browser should zoom out.
  CSSRect targetRect;

  // If we are asked to zoom out but cannot (due to zoom constraints, etc), then
  // zoom in some small amount to provide feedback to the user.
  CantZoomOutBehavior cantZoomOutBehavior = CantZoomOutBehavior::Nothing;

  // If zooming all the way in on |targetRect| is not possible (for example, due
  // to a max zoom constraint), |elementBoundingRect| may be used to inform a
  // more optimal target scroll position (for example, we may try to maximize
  // the area of |elementBoundingRect| that's showing, while keeping
  // |targetRect| in view and keeping the zoom as close to the desired zoom as
  // possible).
  Maybe<CSSRect> elementBoundingRect;

  // The document relative (ie if the content inside the root scroll frame
  // existed without that scroll frame) pointer position at the time of the
  // double tap or location of the double tap if we can compute it. Only used if
  // the rest of this ZoomTarget is asking to zoom out but we are already at the
  // minimum zoom. In which case we zoom in a small amount on this point.
  Maybe<CSSPoint> documentRelativePointerPosition;
};

/**
 * For a double tap at |aPoint|, return a ZoomTarget struct with contains a rect
 * to which the browser should zoom in response (see ZoomTarget definition for
 * more details). An empty rect means the browser should zoom out. |aDocument|
 * should be the root content document for the content that was tapped.
 */
ZoomTarget CalculateRectToZoomTo(
    const RefPtr<mozilla::dom::Document>& aRootContentDocument,
    const CSSPoint& aPoint);

}  // namespace layers
}  // namespace mozilla

#endif /* mozilla_layers_DoubleTapToZoom_h */
