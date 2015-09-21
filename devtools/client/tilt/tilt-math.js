/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cu} = require("chrome");

var TiltUtils = require("devtools/client/tilt/tilt-utils");

/**
 * Module containing high performance matrix and vector operations for WebGL.
 * Inspired by glMatrix, version 0.9.6, (c) 2011 Brandon Jones.
 */

var EPSILON = 0.01;
exports.EPSILON = EPSILON;

const PI_OVER_180 = Math.PI / 180;
const INV_PI_OVER_180 = 180 / Math.PI;
const FIFTEEN_OVER_225 = 15 / 225;
const ONE_OVER_255 = 1 / 255;

/**
 * vec3 - 3 Dimensional Vector.
 */
var vec3 = {

  /**
   * Creates a new instance of a vec3 using the Float32Array type.
   * Any array containing at least 3 numeric elements can serve as a vec3.
   *
   * @param {Array} aVec
   *                optional, vec3 containing values to initialize with
   *
   * @return {Array} a new instance of a vec3
   */
  create: function V3_create(aVec)
  {
    let dest = new Float32Array(3);

    if (aVec) {
      vec3.set(aVec, dest);
    } else {
      vec3.zero(dest);
    }
    return dest;
  },

  /**
   * Copies the values of one vec3 to another.
   *
   * @param {Array} aVec
   *                vec3 containing values to copy
   * @param {Array} aDest
   *                vec3 receiving copied values
   *
   * @return {Array} the destination vec3 receiving copied values
   */
  set: function V3_set(aVec, aDest)
  {
    aDest[0] = aVec[0];
    aDest[1] = aVec[1];
    aDest[2] = aVec[2] || 0;
    return aDest;
  },

  /**
   * Sets a vec3 to an zero vector.
   *
   * @param {Array} aDest
   *                vec3 to set
   *
   * @return {Array} the same vector
   */
  zero: function V3_zero(aDest)
  {
    aDest[0] = 0;
    aDest[1] = 0;
    aDest[2] = 0;
    return aDest;
  },

  /**
   * Performs a vector addition.
   *
   * @param {Array} aVec
   *                vec3, first operand
   * @param {Array} aVec2
   *                vec3, second operand
   * @param {Array} aDest
   *                optional, vec3 receiving operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination vec3 if specified, first operand otherwise
   */
  add: function V3_add(aVec, aVec2, aDest)
  {
    if (!aDest) {
      aDest = aVec;
    }

    aDest[0] = aVec[0] + aVec2[0];
    aDest[1] = aVec[1] + aVec2[1];
    aDest[2] = aVec[2] + aVec2[2];
    return aDest;
  },

  /**
   * Performs a vector subtraction.
   *
   * @param {Array} aVec
   *                vec3, first operand
   * @param {Array} aVec2
   *                vec3, second operand
   * @param {Array} aDest
   *                optional, vec3 receiving operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination vec3 if specified, first operand otherwise
   */
  subtract: function V3_subtract(aVec, aVec2, aDest)
  {
    if (!aDest) {
      aDest = aVec;
    }

    aDest[0] = aVec[0] - aVec2[0];
    aDest[1] = aVec[1] - aVec2[1];
    aDest[2] = aVec[2] - aVec2[2];
    return aDest;
  },

  /**
   * Negates the components of a vec3.
   *
   * @param {Array} aVec
   *                vec3 to negate
   * @param {Array} aDest
   *                optional, vec3 receiving operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination vec3 if specified, first operand otherwise
   */
  negate: function V3_negate(aVec, aDest)
  {
    if (!aDest) {
      aDest = aVec;
    }

    aDest[0] = -aVec[0];
    aDest[1] = -aVec[1];
    aDest[2] = -aVec[2];
    return aDest;
  },

  /**
   * Multiplies the components of a vec3 by a scalar value.
   *
   * @param {Array} aVec
   *                vec3 to scale
   * @param {Number} aVal
   *                 numeric value to scale by
   * @param {Array} aDest
   *                optional, vec3 receiving operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination vec3 if specified, first operand otherwise
   */
  scale: function V3_scale(aVec, aVal, aDest)
  {
    if (!aDest) {
      aDest = aVec;
    }

    aDest[0] = aVec[0] * aVal;
    aDest[1] = aVec[1] * aVal;
    aDest[2] = aVec[2] * aVal;
    return aDest;
  },

  /**
   * Generates a unit vector of the same direction as the provided vec3.
   * If vector length is 0, returns [0, 0, 0].
   *
   * @param {Array} aVec
   *                vec3 to normalize
   * @param {Array} aDest
   *                optional, vec3 receiving operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination vec3 if specified, first operand otherwise
   */
  normalize: function V3_normalize(aVec, aDest)
  {
    if (!aDest) {
      aDest = aVec;
    }

    let x = aVec[0];
    let y = aVec[1];
    let z = aVec[2];
    let len = Math.sqrt(x * x + y * y + z * z);

    if (Math.abs(len) < EPSILON) {
      aDest[0] = 0;
      aDest[1] = 0;
      aDest[2] = 0;
      return aDest;
    }

    len = 1 / len;
    aDest[0] = x * len;
    aDest[1] = y * len;
    aDest[2] = z * len;
    return aDest;
  },

  /**
   * Generates the cross product of two vectors.
   *
   * @param {Array} aVec
   *                vec3, first operand
   * @param {Array} aVec2
   *                vec3, second operand
   * @param {Array} aDest
   *                optional, vec3 receiving operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination vec3 if specified, first operand otherwise
   */
  cross: function V3_cross(aVec, aVec2, aDest)
  {
    if (!aDest) {
      aDest = aVec;
    }

    let x = aVec[0];
    let y = aVec[1];
    let z = aVec[2];
    let x2 = aVec2[0];
    let y2 = aVec2[1];
    let z2 = aVec2[2];

    aDest[0] = y * z2 - z * y2;
    aDest[1] = z * x2 - x * z2;
    aDest[2] = x * y2 - y * x2;
    return aDest;
  },

  /**
   * Caclulate the dot product of two vectors.
   *
   * @param {Array} aVec
   *                vec3, first operand
   * @param {Array} aVec2
   *                vec3, second operand
   *
   * @return {Array} dot product of the first and second operand
   */
  dot: function V3_dot(aVec, aVec2)
  {
    return aVec[0] * aVec2[0] + aVec[1] * aVec2[1] + aVec[2] * aVec2[2];
  },

  /**
   * Caclulate the length of a vec3.
   *
   * @param {Array} aVec
   *                vec3 to calculate length of
   *
   * @return {Array} length of the vec3
   */
  length: function V3_length(aVec)
  {
    let x = aVec[0];
    let y = aVec[1];
    let z = aVec[2];

    return Math.sqrt(x * x + y * y + z * z);
  },

  /**
   * Generates a unit vector pointing from one vector to another.
   *
   * @param {Array} aVec
   *                origin vec3
   * @param {Array} aVec2
   *                vec3 to point to
   * @param {Array} aDest
   *                optional, vec3 receiving operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination vec3 if specified, first operand otherwise
   */
  direction: function V3_direction(aVec, aVec2, aDest)
  {
    if (!aDest) {
      aDest = aVec;
    }

    let x = aVec[0] - aVec2[0];
    let y = aVec[1] - aVec2[1];
    let z = aVec[2] - aVec2[2];
    let len = Math.sqrt(x * x + y * y + z * z);

    if (Math.abs(len) < EPSILON) {
      aDest[0] = 0;
      aDest[1] = 0;
      aDest[2] = 0;
      return aDest;
    }

    len = 1 / len;
    aDest[0] = x * len;
    aDest[1] = y * len;
    aDest[2] = z * len;
    return aDest;
  },

  /**
   * Performs a linear interpolation between two vec3.
   *
   * @param {Array} aVec
   *                first vector
   * @param {Array} aVec2
   *                second vector
   * @param {Number} aLerp
   *                 interpolation amount between the two inputs
   * @param {Array} aDest
   *                optional, vec3 receiving operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination vec3 if specified, first operand otherwise
   */
  lerp: function V3_lerp(aVec, aVec2, aLerp, aDest)
  {
    if (!aDest) {
      aDest = aVec;
    }

    aDest[0] = aVec[0] + aLerp * (aVec2[0] - aVec[0]);
    aDest[1] = aVec[1] + aLerp * (aVec2[1] - aVec[1]);
    aDest[2] = aVec[2] + aLerp * (aVec2[2] - aVec[2]);
    return aDest;
  },

  /**
   * Projects a 3D point on a 2D screen plane.
   *
   * @param {Array} aP
   *                the [x, y, z] coordinates of the point to project
   * @param {Array} aViewport
   *                the viewport [x, y, width, height] coordinates
   * @param {Array} aMvMatrix
   *                the model view matrix
   * @param {Array} aProjMatrix
   *                the projection matrix
   * @param {Array} aDest
   *                optional parameter, the array to write the values to
   *
   * @return {Array} the projected coordinates
   */
  project: function V3_project(aP, aViewport, aMvMatrix, aProjMatrix, aDest)
  {
    /*jshint undef: false */

    let mvpMatrix = new Float32Array(16);
    let coordinates = new Float32Array(4);

    // compute the perspective * model view matrix
    mat4.multiply(aProjMatrix, aMvMatrix, mvpMatrix);

    // now transform that vector into homogenous coordinates
    coordinates[0] = aP[0];
    coordinates[1] = aP[1];
    coordinates[2] = aP[2];
    coordinates[3] = 1;
    mat4.multiplyVec4(mvpMatrix, coordinates);

    // transform the homogenous coordinates into screen space
    coordinates[0] /= coordinates[3];
    coordinates[0] *= aViewport[2] * 0.5;
    coordinates[0] += aViewport[2] * 0.5;
    coordinates[1] /= coordinates[3];
    coordinates[1] *= -aViewport[3] * 0.5;
    coordinates[1] += aViewport[3] * 0.5;
    coordinates[2] = 0;

    if (!aDest) {
      vec3.set(coordinates, aP);
    } else {
      vec3.set(coordinates, aDest);
    }
    return coordinates;
  },

  /**
   * Unprojects a 2D point to 3D space.
   *
   * @param {Array} aP
   *                the [x, y, z] coordinates of the point to unproject;
   *                the z value should range between 0 and 1, as clipping plane
   * @param {Array} aViewport
   *                the viewport [x, y, width, height] coordinates
   * @param {Array} aMvMatrix
   *                the model view matrix
   * @param {Array} aProjMatrix
   *                the projection matrix
   * @param {Array} aDest
   *                optional parameter, the array to write the values to
   *
   * @return {Array} the unprojected coordinates
   */
  unproject: function V3_unproject(
    aP, aViewport, aMvMatrix, aProjMatrix, aDest)
  {
    /*jshint undef: false */

    let mvpMatrix = new Float32Array(16);
    let coordinates = new Float32Array(4);

    // compute the inverse of the perspective * model view matrix
    mat4.multiply(aProjMatrix, aMvMatrix, mvpMatrix);
    mat4.inverse(mvpMatrix);

    // transformation of normalized coordinates (-1 to 1)
    coordinates[0] = +((aP[0] - aViewport[0]) / aViewport[2] * 2 - 1);
    coordinates[1] = -((aP[1] - aViewport[1]) / aViewport[3] * 2 - 1);
    coordinates[2] = 2 * aP[2] - 1;
    coordinates[3] = 1;

    // now transform that vector into space coordinates
    mat4.multiplyVec4(mvpMatrix, coordinates);

    // invert to normalize x, y, and z values
    coordinates[3] = 1 / coordinates[3];
    coordinates[0] *= coordinates[3];
    coordinates[1] *= coordinates[3];
    coordinates[2] *= coordinates[3];

    if (!aDest) {
      vec3.set(coordinates, aP);
    } else {
      vec3.set(coordinates, aDest);
    }
    return coordinates;
  },

  /**
   * Create a ray between two points using the current model view & projection
   * matrices. This is useful when creating a ray destined for 3D picking.
   *
   * @param {Array} aP0
   *                the [x, y, z] coordinates of the first point
   * @param {Array} aP1
   *                the [x, y, z] coordinates of the second point
   * @param {Array} aViewport
   *                the viewport [x, y, width, height] coordinates
   * @param {Array} aMvMatrix
   *                the model view matrix
   * @param {Array} aProjMatrix
   *                the projection matrix
   *
   * @return {Object} a ray object containing the direction vector between
   * the two unprojected points, the position and the lookAt
   */
  createRay: function V3_createRay(aP0, aP1, aViewport, aMvMatrix, aProjMatrix)
  {
    // unproject the two points
    vec3.unproject(aP0, aViewport, aMvMatrix, aProjMatrix, aP0);
    vec3.unproject(aP1, aViewport, aMvMatrix, aProjMatrix, aP1);

    return {
      origin: aP0,
      direction: vec3.normalize(vec3.subtract(aP1, aP0))
    };
  },

  /**
   * Returns a string representation of a vector.
   *
   * @param {Array} aVec
   *                vec3 to represent as a string
   *
   * @return {String} representation of the vector
   */
  str: function V3_str(aVec)
  {
    return '[' + aVec[0] + ", " + aVec[1] + ", " + aVec[2] + ']';
  }
};

exports.vec3 = vec3;

/**
 * mat3 - 3x3 Matrix.
 */
var mat3 = {

  /**
   * Creates a new instance of a mat3 using the Float32Array array type.
   * Any array containing at least 9 numeric elements can serve as a mat3.
   *
   * @param {Array} aMat
   *                optional, mat3 containing values to initialize with
   *
   * @return {Array} a new instance of a mat3
   */
  create: function M3_create(aMat)
  {
    let dest = new Float32Array(9);

    if (aMat) {
      mat3.set(aMat, dest);
    } else {
      mat3.identity(dest);
    }
    return dest;
  },

  /**
   * Copies the values of one mat3 to another.
   *
   * @param {Array} aMat
   *                mat3 containing values to copy
   * @param {Array} aDest
   *                mat3 receiving copied values
   *
   * @return {Array} the destination mat3 receiving copied values
   */
  set: function M3_set(aMat, aDest)
  {
    aDest[0] = aMat[0];
    aDest[1] = aMat[1];
    aDest[2] = aMat[2];
    aDest[3] = aMat[3];
    aDest[4] = aMat[4];
    aDest[5] = aMat[5];
    aDest[6] = aMat[6];
    aDest[7] = aMat[7];
    aDest[8] = aMat[8];
    return aDest;
  },

  /**
   * Sets a mat3 to an identity matrix.
   *
   * @param {Array} aDest
   *                mat3 to set
   *
   * @return {Array} the same matrix
   */
  identity: function M3_identity(aDest)
  {
    aDest[0] = 1;
    aDest[1] = 0;
    aDest[2] = 0;
    aDest[3] = 0;
    aDest[4] = 1;
    aDest[5] = 0;
    aDest[6] = 0;
    aDest[7] = 0;
    aDest[8] = 1;
    return aDest;
  },

  /**
   * Transposes a mat3 (flips the values over the diagonal).
   *
   * @param {Array} aMat
   *                mat3 to transpose
   * @param {Array} aDest
   *                optional, mat3 receiving operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination mat3 if specified, first operand otherwise
   */
  transpose: function M3_transpose(aMat, aDest)
  {
    if (!aDest || aMat === aDest) {
      let a01 = aMat[1];
      let a02 = aMat[2];
      let a12 = aMat[5];

      aMat[1] = aMat[3];
      aMat[2] = aMat[6];
      aMat[3] = a01;
      aMat[5] = aMat[7];
      aMat[6] = a02;
      aMat[7] = a12;
      return aMat;
    }

    aDest[0] = aMat[0];
    aDest[1] = aMat[3];
    aDest[2] = aMat[6];
    aDest[3] = aMat[1];
    aDest[4] = aMat[4];
    aDest[5] = aMat[7];
    aDest[6] = aMat[2];
    aDest[7] = aMat[5];
    aDest[8] = aMat[8];
    return aDest;
  },

  /**
   * Copies the elements of a mat3 into the upper 3x3 elements of a mat4.
   *
   * @param {Array} aMat
   *                mat3 containing values to copy
   * @param {Array} aDest
   *                optional, mat4 receiving operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination mat3 if specified, first operand otherwise
   */
  toMat4: function M3_toMat4(aMat, aDest)
  {
    if (!aDest) {
      aDest = new Float32Array(16);
    }

    aDest[0] = aMat[0];
    aDest[1] = aMat[1];
    aDest[2] = aMat[2];
    aDest[3] = 0;
    aDest[4] = aMat[3];
    aDest[5] = aMat[4];
    aDest[6] = aMat[5];
    aDest[7] = 0;
    aDest[8] = aMat[6];
    aDest[9] = aMat[7];
    aDest[10] = aMat[8];
    aDest[11] = 0;
    aDest[12] = 0;
    aDest[13] = 0;
    aDest[14] = 0;
    aDest[15] = 1;
    return aDest;
  },

  /**
   * Returns a string representation of a 3x3 matrix.
   *
   * @param {Array} aMat
   *                mat3 to represent as a string
   *
   * @return {String} representation of the matrix
   */
  str: function M3_str(aMat)
  {
    return "["  + aMat[0] + ", " + aMat[1] + ", " + aMat[2] +
           ", " + aMat[3] + ", " + aMat[4] + ", " + aMat[5] +
           ", " + aMat[6] + ", " + aMat[7] + ", " + aMat[8] + "]";
  }
};

exports.mat3 = mat3;

/**
 * mat4 - 4x4 Matrix.
 */
var mat4 = {

  /**
   * Creates a new instance of a mat4 using the default Float32Array type.
   * Any array containing at least 16 numeric elements can serve as a mat4.
   *
   * @param {Array} aMat
   *                optional, mat4 containing values to initialize with
   *
   * @return {Array} a new instance of a mat4
   */
  create: function M4_create(aMat)
  {
    let dest = new Float32Array(16);

    if (aMat) {
      mat4.set(aMat, dest);
    } else {
      mat4.identity(dest);
    }
    return dest;
  },

  /**
   * Copies the values of one mat4 to another
   *
   * @param {Array} aMat
   *                mat4 containing values to copy
   * @param {Array} aDest
   *                mat4 receiving copied values
   *
   * @return {Array} the destination mat4 receiving copied values
   */
  set: function M4_set(aMat, aDest)
  {
    aDest[0] = aMat[0];
    aDest[1] = aMat[1];
    aDest[2] = aMat[2];
    aDest[3] = aMat[3];
    aDest[4] = aMat[4];
    aDest[5] = aMat[5];
    aDest[6] = aMat[6];
    aDest[7] = aMat[7];
    aDest[8] = aMat[8];
    aDest[9] = aMat[9];
    aDest[10] = aMat[10];
    aDest[11] = aMat[11];
    aDest[12] = aMat[12];
    aDest[13] = aMat[13];
    aDest[14] = aMat[14];
    aDest[15] = aMat[15];
    return aDest;
  },

  /**
   * Sets a mat4 to an identity matrix.
   *
   * @param {Array} aDest
   *                mat4 to set
   *
   * @return {Array} the same matrix
   */
  identity: function M4_identity(aDest)
  {
    aDest[0] = 1;
    aDest[1] = 0;
    aDest[2] = 0;
    aDest[3] = 0;
    aDest[4] = 0;
    aDest[5] = 1;
    aDest[6] = 0;
    aDest[7] = 0;
    aDest[8] = 0;
    aDest[9] = 0;
    aDest[10] = 1;
    aDest[11] = 0;
    aDest[12] = 0;
    aDest[13] = 0;
    aDest[14] = 0;
    aDest[15] = 1;
    return aDest;
  },

  /**
   * Transposes a mat4 (flips the values over the diagonal).
   *
   * @param {Array} aMat
   *                mat4 to transpose
   * @param {Array} aDest
   *                optional, mat4 receiving operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination mat4 if specified, first operand otherwise
   */
  transpose: function M4_transpose(aMat, aDest)
  {
    if (!aDest || aMat === aDest) {
      let a01 = aMat[1];
      let a02 = aMat[2];
      let a03 = aMat[3];
      let a12 = aMat[6];
      let a13 = aMat[7];
      let a23 = aMat[11];

      aMat[1] = aMat[4];
      aMat[2] = aMat[8];
      aMat[3] = aMat[12];
      aMat[4] = a01;
      aMat[6] = aMat[9];
      aMat[7] = aMat[13];
      aMat[8] = a02;
      aMat[9] = a12;
      aMat[11] = aMat[14];
      aMat[12] = a03;
      aMat[13] = a13;
      aMat[14] = a23;
      return aMat;
    }

    aDest[0] = aMat[0];
    aDest[1] = aMat[4];
    aDest[2] = aMat[8];
    aDest[3] = aMat[12];
    aDest[4] = aMat[1];
    aDest[5] = aMat[5];
    aDest[6] = aMat[9];
    aDest[7] = aMat[13];
    aDest[8] = aMat[2];
    aDest[9] = aMat[6];
    aDest[10] = aMat[10];
    aDest[11] = aMat[14];
    aDest[12] = aMat[3];
    aDest[13] = aMat[7];
    aDest[14] = aMat[11];
    aDest[15] = aMat[15];
    return aDest;
  },

  /**
   * Calculate the determinant of a mat4.
   *
   * @param {Array} aMat
   *                mat4 to calculate determinant of
   *
   * @return {Number} determinant of the matrix
   */
  determinant: function M4_determinant(mat)
  {
    let a00 = mat[0], a01 = mat[1], a02 = mat[2], a03 = mat[3];
    let a10 = mat[4], a11 = mat[5], a12 = mat[6], a13 = mat[7];
    let a20 = mat[8], a21 = mat[9], a22 = mat[10], a23 = mat[11];
    let a30 = mat[12], a31 = mat[13], a32 = mat[14], a33 = mat[15];

    return a30 * a21 * a12 * a03 - a20 * a31 * a12 * a03 -
           a30 * a11 * a22 * a03 + a10 * a31 * a22 * a03 +
           a20 * a11 * a32 * a03 - a10 * a21 * a32 * a03 -
           a30 * a21 * a02 * a13 + a20 * a31 * a02 * a13 +
           a30 * a01 * a22 * a13 - a00 * a31 * a22 * a13 -
           a20 * a01 * a32 * a13 + a00 * a21 * a32 * a13 +
           a30 * a11 * a02 * a23 - a10 * a31 * a02 * a23 -
           a30 * a01 * a12 * a23 + a00 * a31 * a12 * a23 +
           a10 * a01 * a32 * a23 - a00 * a11 * a32 * a23 -
           a20 * a11 * a02 * a33 + a10 * a21 * a02 * a33 +
           a20 * a01 * a12 * a33 - a00 * a21 * a12 * a33 -
           a10 * a01 * a22 * a33 + a00 * a11 * a22 * a33;
  },

  /**
   * Calculate the inverse of a mat4.
   *
   * @param {Array} aMat
   *                mat4 to calculate inverse of
   * @param {Array} aDest
   *                optional, mat4 receiving operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination mat4 if specified, first operand otherwise
   */
  inverse: function M4_inverse(aMat, aDest)
  {
    if (!aDest) {
      aDest = aMat;
    }

    let a00 = aMat[0], a01 = aMat[1], a02 = aMat[2], a03 = aMat[3];
    let a10 = aMat[4], a11 = aMat[5], a12 = aMat[6], a13 = aMat[7];
    let a20 = aMat[8], a21 = aMat[9], a22 = aMat[10], a23 = aMat[11];
    let a30 = aMat[12], a31 = aMat[13], a32 = aMat[14], a33 = aMat[15];

    let b00 = a00 * a11 - a01 * a10;
    let b01 = a00 * a12 - a02 * a10;
    let b02 = a00 * a13 - a03 * a10;
    let b03 = a01 * a12 - a02 * a11;
    let b04 = a01 * a13 - a03 * a11;
    let b05 = a02 * a13 - a03 * a12;
    let b06 = a20 * a31 - a21 * a30;
    let b07 = a20 * a32 - a22 * a30;
    let b08 = a20 * a33 - a23 * a30;
    let b09 = a21 * a32 - a22 * a31;
    let b10 = a21 * a33 - a23 * a31;
    let b11 = a22 * a33 - a23 * a32;
    let id = 1 / ((b00 * b11 - b01 * b10 + b02 * b09 +
                   b03 * b08 - b04 * b07 + b05 * b06) || EPSILON);

    aDest[0] = ( a11 * b11 - a12 * b10 + a13 * b09) * id;
    aDest[1] = (-a01 * b11 + a02 * b10 - a03 * b09) * id;
    aDest[2] = ( a31 * b05 - a32 * b04 + a33 * b03) * id;
    aDest[3] = (-a21 * b05 + a22 * b04 - a23 * b03) * id;
    aDest[4] = (-a10 * b11 + a12 * b08 - a13 * b07) * id;
    aDest[5] = ( a00 * b11 - a02 * b08 + a03 * b07) * id;
    aDest[6] = (-a30 * b05 + a32 * b02 - a33 * b01) * id;
    aDest[7] = ( a20 * b05 - a22 * b02 + a23 * b01) * id;
    aDest[8] = ( a10 * b10 - a11 * b08 + a13 * b06) * id;
    aDest[9] = (-a00 * b10 + a01 * b08 - a03 * b06) * id;
    aDest[10] = ( a30 * b04 - a31 * b02 + a33 * b00) * id;
    aDest[11] = (-a20 * b04 + a21 * b02 - a23 * b00) * id;
    aDest[12] = (-a10 * b09 + a11 * b07 - a12 * b06) * id;
    aDest[13] = ( a00 * b09 - a01 * b07 + a02 * b06) * id;
    aDest[14] = (-a30 * b03 + a31 * b01 - a32 * b00) * id;
    aDest[15] = ( a20 * b03 - a21 * b01 + a22 * b00) * id;
    return aDest;
  },

  /**
   * Copies the upper 3x3 elements of a mat4 into another mat4.
   *
   * @param {Array} aMat
   *                mat4 containing values to copy
   * @param {Array} aDest
   *                optional, mat4 receiving operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination mat4 if specified, first operand otherwise
   */
  toRotationMat: function M4_toRotationMat(aMat, aDest)
  {
    if (!aDest) {
      aDest = new Float32Array(16);
    }

    aDest[0] = aMat[0];
    aDest[1] = aMat[1];
    aDest[2] = aMat[2];
    aDest[3] = aMat[3];
    aDest[4] = aMat[4];
    aDest[5] = aMat[5];
    aDest[6] = aMat[6];
    aDest[7] = aMat[7];
    aDest[8] = aMat[8];
    aDest[9] = aMat[9];
    aDest[10] = aMat[10];
    aDest[11] = aMat[11];
    aDest[12] = 0;
    aDest[13] = 0;
    aDest[14] = 0;
    aDest[15] = 1;
    return aDest;
  },

  /**
   * Copies the upper 3x3 elements of a mat4 into a mat3.
   *
   * @param {Array} aMat
   *                mat4 containing values to copy
   * @param {Array} aDest
   *                optional, mat3 receiving operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination mat3 if specified, first operand otherwise
   */
  toMat3: function M4_toMat3(aMat, aDest)
  {
    if (!aDest) {
      aDest = new Float32Array(9);
    }

    aDest[0] = aMat[0];
    aDest[1] = aMat[1];
    aDest[2] = aMat[2];
    aDest[3] = aMat[4];
    aDest[4] = aMat[5];
    aDest[5] = aMat[6];
    aDest[6] = aMat[8];
    aDest[7] = aMat[9];
    aDest[8] = aMat[10];
    return aDest;
  },

  /**
   * Calculate the inverse of the upper 3x3 elements of a mat4 and copies
   * the result into a mat3. The resulting matrix is useful for calculating
   * transformed normals.
   *
   * @param {Array} aMat
   *                mat4 containing values to invert and copy
   * @param {Array} aDest
   *                optional, mat3 receiving operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination mat3 if specified, first operand otherwise
   */
  toInverseMat3: function M4_toInverseMat3(aMat, aDest)
  {
    if (!aDest) {
      aDest = new Float32Array(9);
    }

    let a00 = aMat[0], a01 = aMat[1], a02 = aMat[2];
    let a10 = aMat[4], a11 = aMat[5], a12 = aMat[6];
    let a20 = aMat[8], a21 = aMat[9], a22 = aMat[10];

    let b01 = a22 * a11 - a12 * a21;
    let b11 = -a22 * a10 + a12 * a20;
    let b21 = a21 * a10 - a11 * a20;
    let id = 1 / ((a00 * b01 + a01 * b11 + a02 * b21) || EPSILON);

    aDest[0] = b01 * id;
    aDest[1] = (-a22 * a01 + a02 * a21) * id;
    aDest[2] = ( a12 * a01 - a02 * a11) * id;
    aDest[3] = b11 * id;
    aDest[4] = ( a22 * a00 - a02 * a20) * id;
    aDest[5] = (-a12 * a00 + a02 * a10) * id;
    aDest[6] = b21 * id;
    aDest[7] = (-a21 * a00 + a01 * a20) * id;
    aDest[8] = ( a11 * a00 - a01 * a10) * id;
    return aDest;
  },

  /**
   * Performs a matrix multiplication.
   *
   * @param {Array} aMat
   *                first operand
   * @param {Array} aMat2
   *                second operand
   * @param {Array} aDest
   *                optional, mat4 receiving operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination mat4 if specified, first operand otherwise
   */
  multiply: function M4_multiply(aMat, aMat2, aDest)
  {
    if (!aDest) {
      aDest = aMat;
    }

    let a00 = aMat[0], a01 = aMat[1], a02 = aMat[2], a03 = aMat[3];
    let a10 = aMat[4], a11 = aMat[5], a12 = aMat[6], a13 = aMat[7];
    let a20 = aMat[8], a21 = aMat[9], a22 = aMat[10], a23 = aMat[11];
    let a30 = aMat[12], a31 = aMat[13], a32 = aMat[14], a33 = aMat[15];

    let b00 = aMat2[0], b01 = aMat2[1], b02 = aMat2[2], b03 = aMat2[3];
    let b10 = aMat2[4], b11 = aMat2[5], b12 = aMat2[6], b13 = aMat2[7];
    let b20 = aMat2[8], b21 = aMat2[9], b22 = aMat2[10], b23 = aMat2[11];
    let b30 = aMat2[12], b31 = aMat2[13], b32 = aMat2[14], b33 = aMat2[15];

    aDest[0] = b00 * a00 + b01 * a10 + b02 * a20 + b03 * a30;
    aDest[1] = b00 * a01 + b01 * a11 + b02 * a21 + b03 * a31;
    aDest[2] = b00 * a02 + b01 * a12 + b02 * a22 + b03 * a32;
    aDest[3] = b00 * a03 + b01 * a13 + b02 * a23 + b03 * a33;
    aDest[4] = b10 * a00 + b11 * a10 + b12 * a20 + b13 * a30;
    aDest[5] = b10 * a01 + b11 * a11 + b12 * a21 + b13 * a31;
    aDest[6] = b10 * a02 + b11 * a12 + b12 * a22 + b13 * a32;
    aDest[7] = b10 * a03 + b11 * a13 + b12 * a23 + b13 * a33;
    aDest[8] = b20 * a00 + b21 * a10 + b22 * a20 + b23 * a30;
    aDest[9] = b20 * a01 + b21 * a11 + b22 * a21 + b23 * a31;
    aDest[10] = b20 * a02 + b21 * a12 + b22 * a22 + b23 * a32;
    aDest[11] = b20 * a03 + b21 * a13 + b22 * a23 + b23 * a33;
    aDest[12] = b30 * a00 + b31 * a10 + b32 * a20 + b33 * a30;
    aDest[13] = b30 * a01 + b31 * a11 + b32 * a21 + b33 * a31;
    aDest[14] = b30 * a02 + b31 * a12 + b32 * a22 + b33 * a32;
    aDest[15] = b30 * a03 + b31 * a13 + b32 * a23 + b33 * a33;
    return aDest;
  },

  /**
   * Transforms a vec3 with the given matrix.
   * 4th vector component is implicitly 1.
   *
   * @param {Array} aMat
   *                mat4 to transform the vector with
   * @param {Array} aVec
   *                vec3 to transform
   * @param {Array} aDest
   *                optional, vec3 receiving operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination vec3 if specified, aVec operand otherwise
   */
  multiplyVec3: function M4_multiplyVec3(aMat, aVec, aDest)
  {
    if (!aDest) {
      aDest = aVec;
    }

    let x = aVec[0];
    let y = aVec[1];
    let z = aVec[2];

    aDest[0] = aMat[0] * x + aMat[4] * y + aMat[8]  * z + aMat[12];
    aDest[1] = aMat[1] * x + aMat[5] * y + aMat[9]  * z + aMat[13];
    aDest[2] = aMat[2] * x + aMat[6] * y + aMat[10] * z + aMat[14];
    return aDest;
  },

  /**
   * Transforms a vec4 with the given matrix.
   *
   * @param {Array} aMat
   *                mat4 to transform the vector with
   * @param {Array} aVec
   *                vec4 to transform
   * @param {Array} aDest
   *                optional, vec4 receiving operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination vec4 if specified, vec4 operand otherwise
   */
  multiplyVec4: function M4_multiplyVec4(aMat, aVec, aDest)
  {
    if (!aDest) {
      aDest = aVec;
    }

    let x = aVec[0];
    let y = aVec[1];
    let z = aVec[2];
    let w = aVec[3];

    aDest[0] = aMat[0] * x + aMat[4] * y + aMat[8]  * z + aMat[12] * w;
    aDest[1] = aMat[1] * x + aMat[5] * y + aMat[9]  * z + aMat[13] * w;
    aDest[2] = aMat[2] * x + aMat[6] * y + aMat[10] * z + aMat[14] * w;
    aDest[3] = aMat[3] * x + aMat[7] * y + aMat[11] * z + aMat[15] * w;
    return aDest;
  },

  /**
   * Translates a matrix by the given vector.
   *
   * @param {Array} aMat
   *                mat4 to translate
   * @param {Array} aVec
   *                vec3 specifying the translation
   * @param {Array} aDest
   *                optional, mat4 receiving operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination mat4 if specified, first operand otherwise
   */
  translate: function M4_translate(aMat, aVec, aDest)
  {
    let x = aVec[0];
    let y = aVec[1];
    let z = aVec[2];

    if (!aDest || aMat === aDest) {
      aMat[12] = aMat[0] * x + aMat[4] * y + aMat[8] * z + aMat[12];
      aMat[13] = aMat[1] * x + aMat[5] * y + aMat[9] * z + aMat[13];
      aMat[14] = aMat[2] * x + aMat[6] * y + aMat[10] * z + aMat[14];
      aMat[15] = aMat[3] * x + aMat[7] * y + aMat[11] * z + aMat[15];
      return aMat;
    }

    let a00 = aMat[0], a01 = aMat[1], a02 = aMat[2], a03 = aMat[3];
    let a10 = aMat[4], a11 = aMat[5], a12 = aMat[6], a13 = aMat[7];
    let a20 = aMat[8], a21 = aMat[9], a22 = aMat[10], a23 = aMat[11];

    aDest[0] = a00;
    aDest[1] = a01;
    aDest[2] = a02;
    aDest[3] = a03;
    aDest[4] = a10;
    aDest[5] = a11;
    aDest[6] = a12;
    aDest[7] = a13;
    aDest[8] = a20;
    aDest[9] = a21;
    aDest[10] = a22;
    aDest[11] = a23;
    aDest[12] = a00 * x + a10 * y + a20 * z + aMat[12];
    aDest[13] = a01 * x + a11 * y + a21 * z + aMat[13];
    aDest[14] = a02 * x + a12 * y + a22 * z + aMat[14];
    aDest[15] = a03 * x + a13 * y + a23 * z + aMat[15];
    return aDest;
  },

  /**
   * Scales a matrix by the given vector.
   *
   * @param {Array} aMat
   *                mat4 to translate
   * @param {Array} aVec
   *                vec3 specifying the scale on each axis
   * @param {Array} aDest
   *                optional, mat4 receiving operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination mat4 if specified, first operand otherwise
   */
  scale: function M4_scale(aMat, aVec, aDest)
  {
    let x = aVec[0];
    let y = aVec[1];
    let z = aVec[2];

    if (!aDest || aMat === aDest) {
      aMat[0] *= x;
      aMat[1] *= x;
      aMat[2] *= x;
      aMat[3] *= x;
      aMat[4] *= y;
      aMat[5] *= y;
      aMat[6] *= y;
      aMat[7] *= y;
      aMat[8] *= z;
      aMat[9] *= z;
      aMat[10] *= z;
      aMat[11] *= z;
      return aMat;
    }

    aDest[0] = aMat[0] * x;
    aDest[1] = aMat[1] * x;
    aDest[2] = aMat[2] * x;
    aDest[3] = aMat[3] * x;
    aDest[4] = aMat[4] * y;
    aDest[5] = aMat[5] * y;
    aDest[6] = aMat[6] * y;
    aDest[7] = aMat[7] * y;
    aDest[8] = aMat[8] * z;
    aDest[9] = aMat[9] * z;
    aDest[10] = aMat[10] * z;
    aDest[11] = aMat[11] * z;
    aDest[12] = aMat[12];
    aDest[13] = aMat[13];
    aDest[14] = aMat[14];
    aDest[15] = aMat[15];
    return aDest;
  },

  /**
   * Rotates a matrix by the given angle around the specified axis.
   * If rotating around a primary axis (x, y, z) one of the specialized
   * rotation functions should be used instead for performance,
   *
   * @param {Array} aMat
   *                mat4 to rotate
   * @param {Number} aAngle
   *                 the angle (in radians) to rotate
   * @param {Array} aAxis
   *                vec3 representing the axis to rotate around
   * @param {Array} aDest
   *                optional, mat4 receiving operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination mat4 if specified, first operand otherwise
   */
  rotate: function M4_rotate(aMat, aAngle, aAxis, aDest)
  {
    let x = aAxis[0];
    let y = aAxis[1];
    let z = aAxis[2];
    let len = 1 / (Math.sqrt(x * x + y * y + z * z) || EPSILON);

    x *= len;
    y *= len;
    z *= len;

    let s = Math.sin(aAngle);
    let c = Math.cos(aAngle);
    let t = 1 - c;

    let a00 = aMat[0], a01 = aMat[1], a02 = aMat[2], a03 = aMat[3];
    let a10 = aMat[4], a11 = aMat[5], a12 = aMat[6], a13 = aMat[7];
    let a20 = aMat[8], a21 = aMat[9], a22 = aMat[10], a23 = aMat[11];

    let b00 = x * x * t + c, b01 = y * x * t + z * s, b02 = z * x * t - y * s;
    let b10 = x * y * t - z * s, b11 = y * y * t + c, b12 = z * y * t + x * s;
    let b20 = x * z * t + y * s, b21 = y * z * t - x * s, b22 = z * z * t + c;

    if (!aDest) {
      aDest = aMat;
    } else if (aMat !== aDest) {
      aDest[12] = aMat[12];
      aDest[13] = aMat[13];
      aDest[14] = aMat[14];
      aDest[15] = aMat[15];
    }

    aDest[0] = a00 * b00 + a10 * b01 + a20 * b02;
    aDest[1] = a01 * b00 + a11 * b01 + a21 * b02;
    aDest[2] = a02 * b00 + a12 * b01 + a22 * b02;
    aDest[3] = a03 * b00 + a13 * b01 + a23 * b02;
    aDest[4] = a00 * b10 + a10 * b11 + a20 * b12;
    aDest[5] = a01 * b10 + a11 * b11 + a21 * b12;
    aDest[6] = a02 * b10 + a12 * b11 + a22 * b12;
    aDest[7] = a03 * b10 + a13 * b11 + a23 * b12;
    aDest[8] = a00 * b20 + a10 * b21 + a20 * b22;
    aDest[9] = a01 * b20 + a11 * b21 + a21 * b22;
    aDest[10] = a02 * b20 + a12 * b21 + a22 * b22;
    aDest[11] = a03 * b20 + a13 * b21 + a23 * b22;
    return aDest;
  },

  /**
   * Rotates a matrix by the given angle around the X axis.
   *
   * @param {Array} aMat
   *                mat4 to rotate
   * @param {Number} aAngle
   *                 the angle (in radians) to rotate
   * @param {Array} aDest
   *                optional, mat4 receiving operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination mat4 if specified, first operand otherwise
   */
  rotateX: function M4_rotateX(aMat, aAngle, aDest)
  {
    let s = Math.sin(aAngle);
    let c = Math.cos(aAngle);

    let a10 = aMat[4], a11 = aMat[5], a12 = aMat[6], a13 = aMat[7];
    let a20 = aMat[8], a21 = aMat[9], a22 = aMat[10], a23 = aMat[11];

    if (!aDest) {
      aDest = aMat;
    } else if (aMat !== aDest) {
      aDest[0] = aMat[0];
      aDest[1] = aMat[1];
      aDest[2] = aMat[2];
      aDest[3] = aMat[3];
      aDest[12] = aMat[12];
      aDest[13] = aMat[13];
      aDest[14] = aMat[14];
      aDest[15] = aMat[15];
    }

    aDest[4] = a10 * c + a20 * s;
    aDest[5] = a11 * c + a21 * s;
    aDest[6] = a12 * c + a22 * s;
    aDest[7] = a13 * c + a23 * s;
    aDest[8] = a10 * -s + a20 * c;
    aDest[9] = a11 * -s + a21 * c;
    aDest[10] = a12 * -s + a22 * c;
    aDest[11] = a13 * -s + a23 * c;
    return aDest;
  },

  /**
   * Rotates a matrix by the given angle around the Y axix.
   *
   * @param {Array} aMat
   *                mat4 to rotate
   * @param {Number} aAngle
   *                 the angle (in radians) to rotate
   * @param {Array} aDest
   *                optional, mat4 receiving operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination mat4 if specified, first operand otherwise
   */
  rotateY: function M4_rotateY(aMat, aAngle, aDest)
  {
    let s = Math.sin(aAngle);
    let c = Math.cos(aAngle);

    let a00 = aMat[0], a01 = aMat[1], a02 = aMat[2], a03 = aMat[3];
    let a20 = aMat[8], a21 = aMat[9], a22 = aMat[10], a23 = aMat[11];

    if (!aDest) {
      aDest = aMat;
    } else if (aMat !== aDest) {
      aDest[4] = aMat[4];
      aDest[5] = aMat[5];
      aDest[6] = aMat[6];
      aDest[7] = aMat[7];
      aDest[12] = aMat[12];
      aDest[13] = aMat[13];
      aDest[14] = aMat[14];
      aDest[15] = aMat[15];
    }

    aDest[0] = a00 * c + a20 * -s;
    aDest[1] = a01 * c + a21 * -s;
    aDest[2] = a02 * c + a22 * -s;
    aDest[3] = a03 * c + a23 * -s;
    aDest[8] = a00 * s + a20 *  c;
    aDest[9] = a01 * s + a21 *  c;
    aDest[10] = a02 * s + a22 *  c;
    aDest[11] = a03 * s + a23 *  c;
    return aDest;
  },

  /**
   * Rotates a matrix by the given angle around the Z axix.
   *
   * @param {Array} aMat
   *                mat4 to rotate
   * @param {Number} aAngle
   *                 the angle (in radians) to rotate
   * @param {Array} aDest
   *                optional, mat4 receiving operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination mat4 if specified, first operand otherwise
   */
  rotateZ: function M4_rotateZ(aMat, aAngle, aDest)
  {
    let s = Math.sin(aAngle);
    let c = Math.cos(aAngle);

    let a00 = aMat[0], a01 = aMat[1], a02 = aMat[2], a03 = aMat[3];
    let a10 = aMat[4], a11 = aMat[5], a12 = aMat[6], a13 = aMat[7];

    if (!aDest) {
      aDest = aMat;
    } else if (aMat !== aDest) {
      aDest[8] = aMat[8];
      aDest[9] = aMat[9];
      aDest[10] = aMat[10];
      aDest[11] = aMat[11];
      aDest[12] = aMat[12];
      aDest[13] = aMat[13];
      aDest[14] = aMat[14];
      aDest[15] = aMat[15];
    }

    aDest[0] = a00 * c + a10 * s;
    aDest[1] = a01 * c + a11 * s;
    aDest[2] = a02 * c + a12 * s;
    aDest[3] = a03 * c + a13 * s;
    aDest[4] = a00 * -s + a10 * c;
    aDest[5] = a01 * -s + a11 * c;
    aDest[6] = a02 * -s + a12 * c;
    aDest[7] = a03 * -s + a13 * c;
    return aDest;
  },

  /**
   * Generates a frustum matrix with the given bounds.
   *
   * @param {Number} aLeft
   *                 scalar, left bound of the frustum
   * @param {Number} aRight
   *                 scalar, right bound of the frustum
   * @param {Number} aBottom
   *                 scalar, bottom bound of the frustum
   * @param {Number} aTop
   *                 scalar, top bound of the frustum
   * @param {Number} aNear
   *                 scalar, near bound of the frustum
   * @param {Number} aFar
   *                 scalar, far bound of the frustum
   * @param {Array} aDest
   *                optional, mat4 frustum matrix will be written into
   *                if not specified result is written to a new mat4
   *
   * @return {Array} the destination mat4 if specified, a new mat4 otherwise
   */
  frustum: function M4_frustum(
    aLeft, aRight, aBottom, aTop, aNear, aFar, aDest)
  {
    if (!aDest) {
      aDest = new Float32Array(16);
    }

    let rl = (aRight - aLeft);
    let tb = (aTop - aBottom);
    let fn = (aFar - aNear);

    aDest[0] = (aNear * 2) / rl;
    aDest[1] = 0;
    aDest[2] = 0;
    aDest[3] = 0;
    aDest[4] = 0;
    aDest[5] = (aNear * 2) / tb;
    aDest[6] = 0;
    aDest[7] = 0;
    aDest[8] = (aRight + aLeft) / rl;
    aDest[9] = (aTop + aBottom) / tb;
    aDest[10] = -(aFar + aNear) / fn;
    aDest[11] = -1;
    aDest[12] = 0;
    aDest[13] = 0;
    aDest[14] = -(aFar * aNear * 2) / fn;
    aDest[15] = 0;
    return aDest;
  },

  /**
   * Generates a perspective projection matrix with the given bounds.
   *
   * @param {Number} aFovy
   *                 scalar, vertical field of view (degrees)
   * @param {Number} aAspect
   *                 scalar, aspect ratio (typically viewport width/height)
   * @param {Number} aNear
   *                 scalar, near bound of the frustum
   * @param {Number} aFar
   *                 scalar, far bound of the frustum
   * @param {Array} aDest
   *                optional, mat4 frustum matrix will be written into
   *                if not specified result is written to a new mat4
   *
   * @return {Array} the destination mat4 if specified, a new mat4 otherwise
   */
  perspective: function M4_perspective(
    aFovy, aAspect, aNear, aFar, aDest, aFlip)
  {
    let upper = aNear * Math.tan(aFovy * 0.00872664626); // PI * 180 / 2
    let right = upper * aAspect;
    let top = upper * (aFlip || 1);

    return mat4.frustum(-right, right, -top, top, aNear, aFar, aDest);
  },

  /**
   * Generates a orthogonal projection matrix with the given bounds.
   *
   * @param {Number} aLeft
   *                 scalar, left bound of the frustum
   * @param {Number} aRight
   *                 scalar, right bound of the frustum
   * @param {Number} aBottom
   *                 scalar, bottom bound of the frustum
   * @param {Number} aTop
   *                 scalar, top bound of the frustum
   * @param {Number} aNear
   *                 scalar, near bound of the frustum
   * @param {Number} aFar
   *                 scalar, far bound of the frustum
   * @param {Array} aDest
   *                optional, mat4 frustum matrix will be written into
   *                if not specified result is written to a new mat4
   *
   * @return {Array} the destination mat4 if specified, a new mat4 otherwise
   */
  ortho: function M4_ortho(aLeft, aRight, aBottom, aTop, aNear, aFar, aDest)
  {
    if (!aDest) {
      aDest = new Float32Array(16);
    }

    let rl = (aRight - aLeft);
    let tb = (aTop - aBottom);
    let fn = (aFar - aNear);

    aDest[0] = 2 / rl;
    aDest[1] = 0;
    aDest[2] = 0;
    aDest[3] = 0;
    aDest[4] = 0;
    aDest[5] = 2 / tb;
    aDest[6] = 0;
    aDest[7] = 0;
    aDest[8] = 0;
    aDest[9] = 0;
    aDest[10] = -2 / fn;
    aDest[11] = 0;
    aDest[12] = -(aLeft + aRight) / rl;
    aDest[13] = -(aTop + aBottom) / tb;
    aDest[14] = -(aFar + aNear) / fn;
    aDest[15] = 1;
    return aDest;
  },

  /**
   * Generates a look-at matrix with the given eye position, focal point, and
   * up axis.
   *
   * @param {Array} aEye
   *                vec3, position of the viewer
   * @param {Array} aCenter
   *                vec3, point the viewer is looking at
   * @param {Array} aUp
   *                vec3 pointing up
   * @param {Array} aDest
   *                optional, mat4 frustum matrix will be written into
   *                if not specified result is written to a new mat4
   *
   * @return {Array} the destination mat4 if specified, a new mat4 otherwise
   */
  lookAt: function M4_lookAt(aEye, aCenter, aUp, aDest)
  {
    if (!aDest) {
      aDest = new Float32Array(16);
    }

    let eyex = aEye[0];
    let eyey = aEye[1];
    let eyez = aEye[2];
    let upx = aUp[0];
    let upy = aUp[1];
    let upz = aUp[2];
    let centerx = aCenter[0];
    let centery = aCenter[1];
    let centerz = aCenter[2];

    let z0 = eyex - aCenter[0];
    let z1 = eyey - aCenter[1];
    let z2 = eyez - aCenter[2];
    let len = 1 / (Math.sqrt(z0 * z0 + z1 * z1 + z2 * z2) || EPSILON);

    z0 *= len;
    z1 *= len;
    z2 *= len;

    let x0 = upy * z2 - upz * z1;
    let x1 = upz * z0 - upx * z2;
    let x2 = upx * z1 - upy * z0;
    len = 1 / (Math.sqrt(x0 * x0 + x1 * x1 + x2 * x2) || EPSILON);

    x0 *= len;
    x1 *= len;
    x2 *= len;

    let y0 = z1 * x2 - z2 * x1;
    let y1 = z2 * x0 - z0 * x2;
    let y2 = z0 * x1 - z1 * x0;
    len = 1 / (Math.sqrt(y0 * y0 + y1 * y1 + y2 * y2) || EPSILON);

    y0 *= len;
    y1 *= len;
    y2 *= len;

    aDest[0] = x0;
    aDest[1] = y0;
    aDest[2] = z0;
    aDest[3] = 0;
    aDest[4] = x1;
    aDest[5] = y1;
    aDest[6] = z1;
    aDest[7] = 0;
    aDest[8] = x2;
    aDest[9] = y2;
    aDest[10] = z2;
    aDest[11] = 0;
    aDest[12] = -(x0 * eyex + x1 * eyey + x2 * eyez);
    aDest[13] = -(y0 * eyex + y1 * eyey + y2 * eyez);
    aDest[14] = -(z0 * eyex + z1 * eyey + z2 * eyez);
    aDest[15] = 1;

    return aDest;
  },

  /**
   * Returns a string representation of a 4x4 matrix.
   *
   * @param {Array} aMat
   *                mat4 to represent as a string
   *
   * @return {String} representation of the matrix
   */
  str: function M4_str(mat)
  {
    return "[" + mat[0] + ", " + mat[1] + ", " + mat[2] + ", " + mat[3] +
           ", "+ mat[4] + ", " + mat[5] + ", " + mat[6] + ", " + mat[7] +
           ", "+ mat[8] + ", " + mat[9] + ", " + mat[10] + ", " + mat[11] +
           ", "+ mat[12] + ", " + mat[13] + ", " + mat[14] + ", " + mat[15] +
           "]";
  }
};

exports.mat4 = mat4;

/**
 * quat4 - Quaternion.
 */
var quat4 = {

  /**
   * Creates a new instance of a quat4 using the default Float32Array type.
   * Any array containing at least 4 numeric elements can serve as a quat4.
   *
   * @param {Array} aQuat
   *                optional, quat4 containing values to initialize with
   *
   * @return {Array} a new instance of a quat4
   */
  create: function Q4_create(aQuat)
  {
    let dest = new Float32Array(4);

    if (aQuat) {
      quat4.set(aQuat, dest);
    } else {
      quat4.identity(dest);
    }
    return dest;
  },

  /**
   * Copies the values of one quat4 to another.
   *
   * @param {Array} aQuat
   *                quat4 containing values to copy
   * @param {Array} aDest
   *                quat4 receiving copied values
   *
   * @return {Array} the destination quat4 receiving copied values
   */
  set: function Q4_set(aQuat, aDest)
  {
    aDest[0] = aQuat[0];
    aDest[1] = aQuat[1];
    aDest[2] = aQuat[2];
    aDest[3] = aQuat[3];
    return aDest;
  },

  /**
   * Sets a quat4 to an identity quaternion.
   *
   * @param {Array} aDest
   *                quat4 to set
   *
   * @return {Array} the same quaternion
   */
  identity: function Q4_identity(aDest)
  {
    aDest[0] = 0;
    aDest[1] = 0;
    aDest[2] = 0;
    aDest[3] = 1;
    return aDest;
  },

  /**
   * Calculate the W component of a quat4 from the X, Y, and Z components.
   * Assumes that quaternion is 1 unit in length.
   * Any existing W component will be ignored.
   *
   * @param {Array} aQuat
   *                quat4 to calculate W component of
   * @param {Array} aDest
   *                optional, quat4 receiving calculated values
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination quat if specified, first operand otherwise
   */
  calculateW: function Q4_calculateW(aQuat, aDest)
  {
    if (!aDest) {
      aDest = aQuat;
    }

    let x = aQuat[0];
    let y = aQuat[1];
    let z = aQuat[2];

    aDest[0] = x;
    aDest[1] = y;
    aDest[2] = z;
    aDest[3] = -Math.sqrt(Math.abs(1 - x * x - y * y - z * z));
    return aDest;
  },

  /**
   * Calculate the inverse of a quat4.
   *
   * @param {Array} aQuat
   *                quat4 to calculate the inverse of
   * @param {Array} aDest
   *                optional, quat4 receiving the inverse values
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination quat if specified, first operand otherwise
   */
  inverse: function Q4_inverse(aQuat, aDest)
  {
    if (!aDest) {
      aDest = aQuat;
    }

    aQuat[0] = -aQuat[0];
    aQuat[1] = -aQuat[1];
    aQuat[2] = -aQuat[2];
    return aQuat;
  },

  /**
   * Generates a unit quaternion of the same direction as the provided quat4.
   * If quaternion length is 0, returns [0, 0, 0, 0].
   *
   * @param {Array} aQuat
   *                quat4 to normalize
   * @param {Array} aDest
   *                optional, quat4 receiving the operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination quat if specified, first operand otherwise
   */
  normalize: function Q4_normalize(aQuat, aDest)
  {
    if (!aDest) {
      aDest = aQuat;
    }

    let x = aQuat[0];
    let y = aQuat[1];
    let z = aQuat[2];
    let w = aQuat[3];
    let len = Math.sqrt(x * x + y * y + z * z + w * w);

    if (Math.abs(len) < EPSILON) {
      aDest[0] = 0;
      aDest[1] = 0;
      aDest[2] = 0;
      aDest[3] = 0;
      return aDest;
    }

    len = 1 / len;
    aDest[0] = x * len;
    aDest[1] = y * len;
    aDest[2] = z * len;
    aDest[3] = w * len;
    return aDest;
  },

  /**
   * Calculate the length of a quat4.
   *
   * @param {Array} aQuat
   *                quat4 to calculate the length of
   *
   * @return {Number} length of the quaternion
   */
  length: function Q4_length(aQuat)
  {
    let x = aQuat[0];
    let y = aQuat[1];
    let z = aQuat[2];
    let w = aQuat[3];

    return Math.sqrt(x * x + y * y + z * z + w * w);
  },

  /**
   * Performs a quaternion multiplication.
   *
   * @param {Array} aQuat
   *                first operand
   * @param {Array} aQuat2
   *                second operand
   * @param {Array} aDest
   *                optional, quat4 receiving the operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination quat if specified, first operand otherwise
   */
  multiply: function Q4_multiply(aQuat, aQuat2, aDest)
  {
    if (!aDest) {
      aDest = aQuat;
    }

    let qax = aQuat[0];
    let qay = aQuat[1];
    let qaz = aQuat[2];
    let qaw = aQuat[3];
    let qbx = aQuat2[0];
    let qby = aQuat2[1];
    let qbz = aQuat2[2];
    let qbw = aQuat2[3];

    aDest[0] = qax * qbw + qaw * qbx + qay * qbz - qaz * qby;
    aDest[1] = qay * qbw + qaw * qby + qaz * qbx - qax * qbz;
    aDest[2] = qaz * qbw + qaw * qbz + qax * qby - qay * qbx;
    aDest[3] = qaw * qbw - qax * qbx - qay * qby - qaz * qbz;
    return aDest;
  },

  /**
   * Transforms a vec3 with the given quaternion.
   *
   * @param {Array} aQuat
   *                quat4 to transform the vector with
   * @param {Array} aVec
   *                vec3 to transform
   * @param {Array} aDest
   *                optional, vec3 receiving the operation result
   *                if not specified result is written to the first operand
   *
   * @return {Array} the destination vec3 if specified, aVec operand otherwise
   */
  multiplyVec3: function Q4_multiplyVec3(aQuat, aVec, aDest)
  {
    if (!aDest) {
      aDest = aVec;
    }

    let x = aVec[0];
    let y = aVec[1];
    let z = aVec[2];

    let qx = aQuat[0];
    let qy = aQuat[1];
    let qz = aQuat[2];
    let qw = aQuat[3];

    let ix = qw * x + qy * z - qz * y;
    let iy = qw * y + qz * x - qx * z;
    let iz = qw * z + qx * y - qy * x;
    let iw = -qx * x - qy * y - qz * z;

    aDest[0] = ix * qw + iw * -qx + iy * -qz - iz * -qy;
    aDest[1] = iy * qw + iw * -qy + iz * -qx - ix * -qz;
    aDest[2] = iz * qw + iw * -qz + ix * -qy - iy * -qx;
    return aDest;
  },

  /**
   * Performs a spherical linear interpolation between two quat4.
   *
   * @param {Array} aQuat
   *                first quaternion
   * @param {Array} aQuat2
   *                second quaternion
   * @param {Number} aSlerp
   *                 interpolation amount between the two inputs
   * @param {Array} aDest
   *                 optional, quat4 receiving the operation result
   *                 if not specified result is written to the first operand
   *
   * @return {Array} the destination quat if specified, first operand otherwise
   */
  slerp: function Q4_slerp(aQuat, aQuat2, aSlerp, aDest)
  {
    if (!aDest) {
      aDest = aQuat;
    }

    let cosHalfTheta = aQuat[0] * aQuat2[0] +
                       aQuat[1] * aQuat2[1] +
                       aQuat[2] * aQuat2[2] +
                       aQuat[3] * aQuat2[3];

    if (Math.abs(cosHalfTheta) >= 1) {
      aDest[0] = aQuat[0];
      aDest[1] = aQuat[1];
      aDest[2] = aQuat[2];
      aDest[3] = aQuat[3];
      return aDest;
    }

    let halfTheta = Math.acos(cosHalfTheta);
    let sinHalfTheta = Math.sqrt(1 - cosHalfTheta * cosHalfTheta);

    if (Math.abs(sinHalfTheta) < EPSILON) {
      aDest[0] = (aQuat[0] * 0.5 + aQuat2[0] * 0.5);
      aDest[1] = (aQuat[1] * 0.5 + aQuat2[1] * 0.5);
      aDest[2] = (aQuat[2] * 0.5 + aQuat2[2] * 0.5);
      aDest[3] = (aQuat[3] * 0.5 + aQuat2[3] * 0.5);
      return aDest;
    }

    let ratioA = Math.sin((1 - aSlerp) * halfTheta) / sinHalfTheta;
    let ratioB = Math.sin(aSlerp * halfTheta) / sinHalfTheta;

    aDest[0] = (aQuat[0] * ratioA + aQuat2[0] * ratioB);
    aDest[1] = (aQuat[1] * ratioA + aQuat2[1] * ratioB);
    aDest[2] = (aQuat[2] * ratioA + aQuat2[2] * ratioB);
    aDest[3] = (aQuat[3] * ratioA + aQuat2[3] * ratioB);
    return aDest;
  },

  /**
   * Calculates a 3x3 matrix from the given quat4.
   *
   * @param {Array} aQuat
   *                quat4 to create matrix from
   * @param {Array} aDest
   *                optional, mat3 receiving the initialization result
   *                if not specified, a new matrix is created
   *
   * @return {Array} the destination mat3 if specified, first operand otherwise
   */
  toMat3: function Q4_toMat3(aQuat, aDest)
  {
    if (!aDest) {
      aDest = new Float32Array(9);
    }

    let x = aQuat[0];
    let y = aQuat[1];
    let z = aQuat[2];
    let w = aQuat[3];

    let x2 = x + x;
    let y2 = y + y;
    let z2 = z + z;
    let xx = x * x2;
    let xy = x * y2;
    let xz = x * z2;
    let yy = y * y2;
    let yz = y * z2;
    let zz = z * z2;
    let wx = w * x2;
    let wy = w * y2;
    let wz = w * z2;

    aDest[0] = 1 - (yy + zz);
    aDest[1] = xy - wz;
    aDest[2] = xz + wy;
    aDest[3] = xy + wz;
    aDest[4] = 1 - (xx + zz);
    aDest[5] = yz - wx;
    aDest[6] = xz - wy;
    aDest[7] = yz + wx;
    aDest[8] = 1 - (xx + yy);
    return aDest;
  },

  /**
   * Calculates a 4x4 matrix from the given quat4.
   *
   * @param {Array} aQuat
   *                quat4 to create matrix from
   * @param {Array} aDest
   *                optional, mat4 receiving the initialization result
   *                if not specified, a new matrix is created
   *
   * @return {Array} the destination mat4 if specified, first operand otherwise
   */
  toMat4: function Q4_toMat4(aQuat, aDest)
  {
    if (!aDest) {
      aDest = new Float32Array(16);
    }

    let x = aQuat[0];
    let y = aQuat[1];
    let z = aQuat[2];
    let w = aQuat[3];

    let x2 = x + x;
    let y2 = y + y;
    let z2 = z + z;
    let xx = x * x2;
    let xy = x * y2;
    let xz = x * z2;
    let yy = y * y2;
    let yz = y * z2;
    let zz = z * z2;
    let wx = w * x2;
    let wy = w * y2;
    let wz = w * z2;

    aDest[0] = 1 - (yy + zz);
    aDest[1] = xy - wz;
    aDest[2] = xz + wy;
    aDest[3] = 0;
    aDest[4] = xy + wz;
    aDest[5] = 1 - (xx + zz);
    aDest[6] = yz - wx;
    aDest[7] = 0;
    aDest[8] = xz - wy;
    aDest[9] = yz + wx;
    aDest[10] = 1 - (xx + yy);
    aDest[11] = 0;
    aDest[12] = 0;
    aDest[13] = 0;
    aDest[14] = 0;
    aDest[15] = 1;
    return aDest;
  },

  /**
   * Creates a rotation quaternion from axis-angle.
   * This function expects that the axis is a normalized vector.
   *
   * @param {Array} aAxis
   *                an array of elements representing the [x, y, z] axis
   * @param {Number} aAngle
   *                 the angle of rotation
   * @param {Array} aDest
   *                optional, quat4 receiving the initialization result
   *                if not specified, a new quaternion is created
   *
   * @return {Array} the quaternion as [x, y, z, w]
   */
  fromAxis: function Q4_fromAxis(aAxis, aAngle, aDest)
  {
    if (!aDest) {
      aDest = new Float32Array(4);
    }

    let ang = aAngle * 0.5;
    let sin = Math.sin(ang);
    let cos = Math.cos(ang);

    aDest[0] = aAxis[0] * sin;
    aDest[1] = aAxis[1] * sin;
    aDest[2] = aAxis[2] * sin;
    aDest[3] = cos;
    return aDest;
  },

  /**
   * Creates a rotation quaternion from Euler angles.
   *
   * @param {Number} aYaw
   *                 the yaw angle of rotation
   * @param {Number} aPitch
   *                 the pitch angle of rotation
   * @param {Number} aRoll
   *                 the roll angle of rotation
   * @param {Array} aDest
   *                optional, quat4 receiving the initialization result
   *                if not specified, a new quaternion is created
   *
   * @return {Array} the quaternion as [x, y, z, w]
   */
  fromEuler: function Q4_fromEuler(aYaw, aPitch, aRoll, aDest)
  {
    if (!aDest) {
      aDest = new Float32Array(4);
    }

    let x = aPitch * 0.5;
    let y = aYaw * 0.5;
    let z = aRoll * 0.5;

    let sinr = Math.sin(x);
    let sinp = Math.sin(y);
    let siny = Math.sin(z);
    let cosr = Math.cos(x);
    let cosp = Math.cos(y);
    let cosy = Math.cos(z);

    aDest[0] = sinr * cosp * cosy - cosr * sinp * siny;
    aDest[1] = cosr * sinp * cosy + sinr * cosp * siny;
    aDest[2] = cosr * cosp * siny - sinr * sinp * cosy;
    aDest[3] = cosr * cosp * cosy + sinr * sinp * siny;
    return aDest;
  },

  /**
   * Returns a string representation of a quaternion.
   *
   * @param {Array} aQuat
   *                quat4 to represent as a string
   *
   * @return {String} representation of the quaternion
   */
  str: function Q4_str(aQuat) {
    return "[" + aQuat[0] + ", " +
                 aQuat[1] + ", " +
                 aQuat[2] + ", " +
                 aQuat[3] + "]";
  }
};

exports.quat4 = quat4;

/**
 * Various algebraic math functions required by the engine.
 */
var TiltMath = {

  /**
   * Helper function, converts degrees to radians.
   *
   * @param {Number} aDegrees
   *                 the degrees to be converted to radians
   *
   * @return {Number} the degrees converted to radians
   */
  radians: function TM_radians(aDegrees)
  {
    return aDegrees * PI_OVER_180;
  },

  /**
   * Helper function, converts radians to degrees.
   *
   * @param {Number} aRadians
   *                 the radians to be converted to degrees
   *
   * @return {Number} the radians converted to degrees
   */
  degrees: function TM_degrees(aRadians)
  {
    return aRadians * INV_PI_OVER_180;
  },

  /**
   * Re-maps a number from one range to another.
   *
   * @param {Number} aValue
   *                 the number to map
   * @param {Number} aLow1
   *                 the normal lower bound of the number
   * @param {Number} aHigh1
   *                 the normal upper bound of the number
   * @param {Number} aLow2
   *                 the new lower bound of the number
   * @param {Number} aHigh2
   *                 the new upper bound of the number
   *
   * @return {Number} the remapped number
   */
  map: function TM_map(aValue, aLow1, aHigh1, aLow2, aHigh2)
  {
    return aLow2 + (aHigh2 - aLow2) * ((aValue - aLow1) / (aHigh1 - aLow1));
  },

  /**
   * Returns if number is power of two.
   *
   * @param {Number} aNumber
   *                 the number to be verified
   *
   * @return {Boolean} true if x is power of two
   */
  isPowerOfTwo: function TM_isPowerOfTwo(aNumber)
  {
    return !(aNumber & (aNumber - 1));
  },

  /**
   * Returns the next closest power of two greater than a number.
   *
   * @param {Number} aNumber
   *                 the number to be converted
   *
   * @return {Number} the next closest power of two for x
   */
  nextPowerOfTwo: function TM_nextPowerOfTwo(aNumber)
  {
    --aNumber;

    for (let i = 1; i < 32; i <<= 1) {
      aNumber = aNumber | aNumber >> i;
    }
    return aNumber + 1;
  },

  /**
   * A convenient way of limiting values to a set boundary.
   *
   * @param {Number} aValue
   *                 the number to be limited
   * @param {Number} aMin
   *                 the minimum allowed value for the number
   * @param {Number} aMax
   *                 the maximum allowed value for the number
   */
  clamp: function TM_clamp(aValue, aMin, aMax)
  {
    return Math.max(aMin, Math.min(aMax, aValue));
  },

  /**
   * Convenient way to clamp a value to 0..1
   *
   * @param {Number} aValue
   *                 the number to be limited
   */
  saturate: function TM_saturate(aValue)
  {
    return Math.max(0, Math.min(1, aValue));
  },

  /**
   * Converts a hex color to rgba.
   * If the passed param is invalid, it will be converted to [0, 0, 0, 1];
   *
   * @param {String} aColor
   *                 color expressed in hex, or using rgb() or rgba()
   *
   * @return {Array} with 4 color 0..1 components: [red, green, blue, alpha]
   */
  hex2rgba: (function()
  {
    let cache = {};

    return function TM_hex2rgba(aColor) {
      let hex = aColor.charAt(0) === "#" ? aColor.substring(1) : aColor;

      // check the cache to see if this color wasn't converted already
      if (cache[hex] !== undefined) {
        return cache[hex];
      }

      // e.g. "f00"
      if (hex.length === 3) {
        let r = parseInt(hex.substring(0, 1), 16) * FIFTEEN_OVER_225;
        let g = parseInt(hex.substring(1, 2), 16) * FIFTEEN_OVER_225;
        let b = parseInt(hex.substring(2, 3), 16) * FIFTEEN_OVER_225;

        return (cache[hex] = [r, g, b, 1]);
      }
      // e.g. "f008"
      if (hex.length === 4) {
        let r = parseInt(hex.substring(0, 1), 16) * FIFTEEN_OVER_225;
        let g = parseInt(hex.substring(1, 2), 16) * FIFTEEN_OVER_225;
        let b = parseInt(hex.substring(2, 3), 16) * FIFTEEN_OVER_225;
        let a = parseInt(hex.substring(3, 4), 16) * FIFTEEN_OVER_225;

        return (cache[hex] = [r, g, b, a]);
      }
      // e.g. "ff0000"
      if (hex.length === 6) {
        let r = parseInt(hex.substring(0, 2), 16) * ONE_OVER_255;
        let g = parseInt(hex.substring(2, 4), 16) * ONE_OVER_255;
        let b = parseInt(hex.substring(4, 6), 16) * ONE_OVER_255;
        let a = 1;

        return (cache[hex] = [r, g, b, a]);
      }
      // e.g "ff0000aa"
      if (hex.length === 8) {
        let r = parseInt(hex.substring(0, 2), 16) * ONE_OVER_255;
        let g = parseInt(hex.substring(2, 4), 16) * ONE_OVER_255;
        let b = parseInt(hex.substring(4, 6), 16) * ONE_OVER_255;
        let a = parseInt(hex.substring(6, 8), 16) * ONE_OVER_255;

        return (cache[hex] = [r, g, b, a]);
      }
      // e.g. "rgba(255, 0, 0, 0.5)"
      if (hex.match("^rgba")) {
        let rgba = hex.substring(5, hex.length - 1).split(",");
        rgba[0] *= ONE_OVER_255;
        rgba[1] *= ONE_OVER_255;
        rgba[2] *= ONE_OVER_255;
        // in CSS, the alpha component of rgba() is already in the range 0..1

        return (cache[hex] = rgba);
      }
      // e.g. "rgb(255, 0, 0)"
      if (hex.match("^rgb")) {
        let rgba = hex.substring(4, hex.length - 1).split(",");
        rgba[0] *= ONE_OVER_255;
        rgba[1] *= ONE_OVER_255;
        rgba[2] *= ONE_OVER_255;
        rgba[3] = 1;

        return (cache[hex] = rgba);
      }

      // your argument is invalid
      return (cache[hex] = [0, 0, 0, 1]);
    };
  }())
};

exports.TiltMath = TiltMath;

// bind the owner object to the necessary functions
TiltUtils.bindObjectFunc(vec3);
TiltUtils.bindObjectFunc(mat3);
TiltUtils.bindObjectFunc(mat4);
TiltUtils.bindObjectFunc(quat4);
TiltUtils.bindObjectFunc(TiltMath);
