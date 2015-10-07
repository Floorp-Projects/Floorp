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

namespace mozilla {

namespace dom {
struct BoxQuadOptions;
struct ConvertCoordinateOptions;
class DOMQuad;
class DOMRectReadOnly;
class DOMPoint;
struct DOMPointInit;
class OwningTextOrElementOrDocument;
class TextOrElementOrDocument;
} // namespace dom

typedef dom::TextOrElementOrDocument GeometryNode;
typedef dom::OwningTextOrElementOrDocument OwningGeometryNode;

/**
 * Computes quads for aNode using aOptions, according to GeometryUtils.getBoxQuads.
 * May set an error in aRv.
 */
void GetBoxQuads(nsINode* aNode,
                 const dom::BoxQuadOptions& aOptions,
                 nsTArray<nsRefPtr<dom::DOMQuad> >& aResult,
                 ErrorResult& aRv);

already_AddRefed<dom::DOMQuad>
ConvertQuadFromNode(nsINode* aTo, dom::DOMQuad& aQuad,
                    const GeometryNode& aFrom,
                    const dom::ConvertCoordinateOptions& aOptions,
                    ErrorResult& aRv);

already_AddRefed<dom::DOMQuad>
ConvertRectFromNode(nsINode* aTo, dom::DOMRectReadOnly& aRect,
                    const GeometryNode& aFrom,
                    const dom::ConvertCoordinateOptions& aOptions,
                    ErrorResult& aRv);

already_AddRefed<dom::DOMPoint>
ConvertPointFromNode(nsINode* aTo, const dom::DOMPointInit& aPoint,
                     const GeometryNode& aFrom,
                     const dom::ConvertCoordinateOptions& aOptions,
                     ErrorResult& aRv);

} // namespace mozilla

#endif /* MOZILLA_GEOMETRYUTILS_H_ */
