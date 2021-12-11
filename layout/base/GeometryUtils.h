/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GEOMETRYUTILS_H_
#define MOZILLA_GEOMETRYUTILS_H_

#include "nsTArray.h"
#include "nsCOMPtr.h"

/**
 * This file defines utility functions for converting between layout
 * coordinate systems.
 */

class nsINode;

namespace mozilla {
class ErrorResult;

namespace dom {
struct BoxQuadOptions;
struct ConvertCoordinateOptions;
class DOMQuad;
class DOMRectReadOnly;
class DOMPoint;
struct DOMPointInit;
class OwningTextOrElementOrDocument;
class TextOrElementOrDocument;
enum class CallerType : uint32_t;
}  // namespace dom

typedef dom::TextOrElementOrDocument GeometryNode;
typedef dom::OwningTextOrElementOrDocument OwningGeometryNode;

/**
 * Computes quads for aNode using aOptions, according to
 * GeometryUtils.getBoxQuads. May set an error in aRv.
 */
void GetBoxQuads(nsINode* aNode, const dom::BoxQuadOptions& aOptions,
                 nsTArray<RefPtr<dom::DOMQuad> >& aResult,
                 dom::CallerType aCallerType, ErrorResult& aRv);

void GetBoxQuadsFromWindowOrigin(nsINode* aNode,
                                 const dom::BoxQuadOptions& aOptions,
                                 nsTArray<RefPtr<dom::DOMQuad> >& aResult,
                                 ErrorResult& aRv);

already_AddRefed<dom::DOMQuad> ConvertQuadFromNode(
    nsINode* aTo, dom::DOMQuad& aQuad, const GeometryNode& aFrom,
    const dom::ConvertCoordinateOptions& aOptions, dom::CallerType aCallerType,
    ErrorResult& aRv);

already_AddRefed<dom::DOMQuad> ConvertRectFromNode(
    nsINode* aTo, dom::DOMRectReadOnly& aRect, const GeometryNode& aFrom,
    const dom::ConvertCoordinateOptions& aOptions, dom::CallerType aCallerType,
    ErrorResult& aRv);

already_AddRefed<dom::DOMPoint> ConvertPointFromNode(
    nsINode* aTo, const dom::DOMPointInit& aPoint, const GeometryNode& aFrom,
    const dom::ConvertCoordinateOptions& aOptions, dom::CallerType aCallerType,
    ErrorResult& aRv);

}  // namespace mozilla

#endif /* MOZILLA_GEOMETRYUTILS_H_ */
