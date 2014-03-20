/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GEOMETRYUTILS_H_
#define MOZILLA_GEOMETRYUTILS_H_

#include "mozilla/ErrorResult.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"

/**
 * This file defines utility functions for converting between layout
 * coordinate systems.
 */

class nsINode;
class nsIDocument;

namespace mozilla {

namespace dom {
struct BoxQuadOptions;
class DOMQuad;
class Element;
class Text;
}

/**
 * Computes quads for aNode using aOptions, according to GeometryUtils.getBoxQuads.
 * May set an error in aRv.
 */
void GetBoxQuads(nsINode* aNode,
                 const dom::BoxQuadOptions& aOptions,
                 nsTArray<nsRefPtr<dom::DOMQuad> >& aResult,
                 ErrorResult& aRv);

}

#endif /* MOZILLA_GEOMETRYUTILS_H_ */
