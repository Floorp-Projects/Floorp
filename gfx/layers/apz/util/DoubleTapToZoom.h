/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_DoubleTapToZoom_h
#define mozilla_layers_DoubleTapToZoom_h

#include "Units.h"

class nsIDocument;
template<class T> class nsCOMPtr;

namespace mozilla {
namespace layers {

/**
 * For a double tap at |aPoint|, return the rect to which the browser
 * should zoom in response, or an empty rect if the browser should zoom out.
 * |aDocument| should be the root content document for the content that was
 * tapped.
 */
CSSRect CalculateRectToZoomTo(const nsCOMPtr<nsIDocument>& aRootContentDocument,
                              const CSSPoint& aPoint);

}
}

#endif /* mozilla_layers_DoubleTapToZoom_h */
