/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_MATRIX_FWD_H_
#define MOZILLA_GFX_MATRIX_FWD_H_

// Forward declare enough things to define the typedefs |Matrix| and
// |Matrix4x4|.

namespace mozilla {
namespace gfx {

template <class T>
class BaseMatrix;

typedef float Float;
typedef BaseMatrix<Float> Matrix;

typedef double Double;
typedef BaseMatrix<Double> MatrixDouble;

struct UnknownUnits;

template <class SourceUnits, class TargetUnits, class T = Float>
class Matrix4x4Typed;
template <class SourceUnits, class TargetUnits>
class Matrix4x4TypedFlagged;

typedef Matrix4x4Typed<UnknownUnits, UnknownUnits> Matrix4x4;
typedef Matrix4x4Typed<UnknownUnits, UnknownUnits, double> Matrix4x4Double;
typedef Matrix4x4TypedFlagged<UnknownUnits, UnknownUnits> Matrix4x4Flagged;

}  // namespace gfx
}  // namespace mozilla

#endif
