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
 * Returns the transformation origin point for the given node.
 *
 * @param {DOMNode} node
 *        The node.
 * @return {Array}
 *        The transformation origin point.
 */
function getNodeTransformOrigin(node) {
  let origin = node.ownerGlobal.getComputedStyle(node).transformOrigin;

  return origin.split(/ /).map(parseFloat);
}
exports.getNodeTransformOrigin = getNodeTransformOrigin;

/**
 * Returns the transformation matrix for the given node.
 *
 * @param {DOMNode} node
 *        The node.
 * @return {Array}
 *        The transformation matrix.
 */
function getNodeTransformationMatrix(node) {
  let t = node.ownerGlobal.getComputedStyle(node).transform;

  if (t === "none") {
    return null;
  }

  // We're assuming is a 2d matrix.
  let m = t.substring(t.indexOf("(") + 1, t.length - 1).split(/,\s*/).map(Number);
  let [a, b, c, d, e, f] = m;

  // If the length is 16, it's a 3d matrix: in that case we'll extrapolate only the values
  // we need for the 2D transformation; this cover the scenario where 3D CSS properties
  // are used only for HW acceleration on 2D transformation.
  if (m.length === 16) {
    c = m[4];
    d = m[5];
    e = m[12];
    f = m[13];
  }

  return [
    a, c, e,
    b, d, f,
    0, 0, 1
  ];
}

exports.getNodeTransformationMatrix = getNodeTransformationMatrix;
