/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Returns a matrix for the scaling given.
 * Calling `scale()` or `scale(1) returns a new identity matrix.
 *
 * @param {Number} [sx = 1]
 *        the abscissa of the scaling vector.
 *        If unspecified, it will equal to `1`.
 * @param {Number} [sy = sx]
 *        The ordinate of the scaling vector.
 *        If not present, its default value is `sx`, leading to a uniform scaling.
 * @return {Array}
 *         The new matrix.
 */
const scale = (sx = 1, sy = sx) => [
  sx, 0, 0,
  0, sy, 0,
  0, 0, 1
];
exports.scale = scale;

/**
 * Returns a matrix for the translation given.
 * Calling `translate()` or `translate(0) returns a new identity matrix.
 *
 * @param {Number} [tx = 0]
 *        The abscissa of the translating vector.
 *        If unspecified, it will equal to `0`.
 * @param {Number} [ty = tx]
 *        The ordinate of the translating vector.
 *        If unspecified, it will equal to `tx`.
 * @return {Array}
 *         The new matrix.
 */
const translate = (tx = 0, ty = tx) => [
  1, 0, tx,
  0, 1, ty,
  0, 0, 1
];
exports.translate = translate;

/**
 * Returns a matrix that reflects about the Y axis.  For example, the point (x1, y1) would
 * become (-x1, y1).
 *
 * @return {Array}
 *         The new matrix.
 */
const reflectAboutY = () => [
  -1, 0, 0,
  0,  1, 0,
  0,  0, 1,
];
exports.reflectAboutY = reflectAboutY;

/**
 * Returns a matrix for the rotation given.
 * Calling `rotate()` or `rotate(0)` returns a new identity matrix.
 *
 * @param {Number} [angle = 0]
 *        The angle, in radians, for which to return a corresponding rotation matrix.
 *        If unspecified, it will equal `0`.
 * @return {Array}
 *         The new matrix.
 */
const rotate = (angle = 0) => {
  let cos = Math.cos(angle);
  let sin = Math.sin(angle);

  return [
    cos,  sin, 0,
    -sin, cos, 0,
    0,    0,   1
  ];
};
exports.rotate = rotate;

/**
 * Returns a new identity matrix.
 *
 * @return {Array}
 *         The new matrix.
 */
const identity = () => [
  1, 0, 0,
  0, 1, 0,
  0, 0, 1
];
exports.identity = identity;

/**
 * Multiplies two matrices and returns a new matrix with the result.
 *
 * @param {Array} M1
 *        The first operand.
 * @param {Array} M2
 *        The second operand.
 * @return {Array}
 *        The resulting matrix.
 */
const multiply = (M1, M2) => {
  let c11 = M1[0] * M2[0] + M1[1] * M2[3] + M1[2] * M2[6];
  let c12 = M1[0] * M2[1] + M1[1] * M2[4] + M1[2] * M2[7];
  let c13 = M1[0] * M2[2] + M1[1] * M2[5] + M1[2] * M2[8];

  let c21 = M1[3] * M2[0] + M1[4] * M2[3] + M1[5] * M2[6];
  let c22 = M1[3] * M2[1] + M1[4] * M2[4] + M1[5] * M2[7];
  let c23 = M1[3] * M2[2] + M1[4] * M2[5] + M1[5] * M2[8];

  let c31 = M1[6] * M2[0] + M1[7] * M2[3] + M1[8] * M2[6];
  let c32 = M1[6] * M2[1] + M1[7] * M2[4] + M1[8] * M2[7];
  let c33 = M1[6] * M2[2] + M1[7] * M2[5] + M1[8] * M2[8];

  return [
    c11, c12, c13,
    c21, c22, c23,
    c31, c32, c33
  ];
};
exports.multiply = multiply;

/**
 * Applies the given matrix to a point.
 *
 * @param {Array} M
 *        The matrix to apply.
 * @param {Array} P
 *        The point's vector.
 * @return {Array}
 *        The resulting point's vector.
 */
const apply = (M, P) => [
  M[0] * P[0] + M[1] * P[1] + M[2],
  M[3] * P[0] + M[4] * P[1] + M[5],
];
exports.apply = apply;

/**
 * Returns `true` if the given matrix is a identity matrix.
 *
 * @param {Array} M
 *        The matrix to check
 * @return {Boolean}
 *        `true` if the matrix passed is a identity matrix, `false` otherwise.
 */
const isIdentity = (M) =>
  M[0] === 1 && M[1] === 0 && M[2] === 0 &&
  M[3] === 0 && M[4] === 1 && M[5] === 0 &&
  M[6] === 0 && M[7] === 0 && M[8] === 1;
exports.isIdentity = isIdentity;

/**
 * Get the change of basis matrix and inverted change of basis matrix
 * for the coordinate system based on the two given vectors, as well as
 * the lengths of the two given vectors.
 *
 * @param {Array} u
 *        The first vector, serving as the "x axis" of the coordinate system.
 * @param {Array} v
 *        The second vector, serving as the "y axis" of the coordinate system.
 * @return {Object}
 *        { basis, invertedBasis, uLength, vLength }
 *        basis and invertedBasis are the change of basis matrices. uLength and
 *        vLength are the lengths of u and v.
 */
const getBasis = (u, v) => {
  let uLength = Math.abs(Math.sqrt(u[0] ** 2 + u[1] ** 2));
  let vLength = Math.abs(Math.sqrt(v[0] ** 2 + v[1] ** 2));
  let basis =
    [ u[0] / uLength, v[0] / vLength, 0,
      u[1] / uLength, v[1] / vLength, 0,
      0,                0,                1 ];
  let determinant = 1 / (basis[0] * basis[4] - basis[1] * basis[3]);
  let invertedBasis =
    [ basis[4] / determinant,  -basis[1] / determinant, 0,
      -basis[3] / determinant,  basis[0] / determinant, 0,
      0,                        0,                      1 ];
  return { basis, invertedBasis, uLength, vLength };
};
exports.getBasis = getBasis;

/**
 * Convert the given matrix to a new coordinate system, based on the change of basis
 * matrix.
 *
 * @param {Array} M
 *        The matrix to convert
 * @param {Array} basis
 *        The change of basis matrix
 * @param {Array} invertedBasis
 *        The inverted change of basis matrix
 * @return {Array}
 *        The converted matrix.
 */
const changeMatrixBase = (M, basis, invertedBasis) => {
  return multiply(invertedBasis, multiply(M, basis));
};
exports.changeMatrixBase = changeMatrixBase;

/**
 * Returns the transformation matrix for the given node, relative to the ancestor passed
 * as second argument; considering the ancestor transformation too.
 * If no ancestor is specified, it will returns the transformation matrix relative to the
 * node's parent element.
 *
 * @param {DOMNode} node
 *        The node.
 * @param {DOMNode} ancestor
 *        The ancestor of the node given.
 ** @return {Array}
 *        The transformation matrix.
 */
function getNodeTransformationMatrix(node, ancestor = node.parentElement) {
  let { a, b, c, d, e, f } = ancestor.getTransformToParent()
                                     .multiply(node.getTransformToAncestor(ancestor));

  return [
    a, c, e,
    b, d, f,
    0, 0, 1
  ];
}
exports.getNodeTransformationMatrix = getNodeTransformationMatrix;
