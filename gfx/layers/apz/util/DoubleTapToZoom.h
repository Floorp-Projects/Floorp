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

/**
 * For a double tap at |aPoint|, return the rect to which the browser
 * should zoom in response, or an empty rect if the browser should zoom out.
 * |aDocument| should be the root content document for the content that was
 * tapped.
 */
CSSRect CalculateRectToZoomTo(
    const RefPtr<mozilla::dom::Document>& aRootContentDocument,
    const CSSPoint& aPoint);

}  // namespace layers
}  // namespace mozilla

#endif /* mozilla_layers_DoubleTapToZoom_h */
