/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function test() {
  let m1 = mat4.create();

  ok(m1, "Should have created a matrix with mat4.create().");
  is(m1.length, 16, "A mat4 should have 16 elements.");

  ok(isApproxVec(m1, [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]),
    "When created, a mat4 should have the values default to identity.");

  mat4.set([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], m1);
  ok(isApproxVec(m1, [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]),
    "The mat4.set() function didn't set the values correctly.");

  mat4.transpose(m1);
  ok(isApproxVec(m1, [1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15, 4, 8, 12, 16]),
    "The mat4.transpose() function didn't set the values correctly.");

  mat4.identity(m1);
  ok(isApproxVec(m1, [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]),
    "The mat4.identity() function didn't set the values correctly.");

  ok(isApprox(mat4.determinant(m1), 1),
    "The mat4.determinant() function didn't calculate the value correctly.");

  let m2 = mat4.inverse([1, 3, 1, 1, 1, 1, 2, 1, 2, 3, 4, 1, 1, 1, 1, 1]);
  ok(isApproxVec(m2, [
    -1, -3, 1, 3, 0.5, 0, 0, -0.5, 0, 1, 0, -1, 0.5, 2, -1, -0.5
  ]), "The mat4.inverse() function didn't calculate the values correctly.");

  let m3 = mat4.toRotationMat([
    1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15, 4, 8, 12, 16]);
  ok(isApproxVec(m3, [
    1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15, 0, 0, 0, 1
  ]), "The mat4.toRotationMat() func. didn't calculate the values correctly.");

  let m4 = mat4.toMat3([
    1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15, 4, 8, 12, 16]);
  ok(isApproxVec(m4, [1, 5, 9, 2, 6, 10, 3, 7, 11]),
    "The mat4.toMat3() function didn't set the values correctly.");

  let m5 = mat4.toInverseMat3([
    1, 3, 1, 1, 1, 1, 2, 1, 2, 3, 4, 1, 1, 1, 1, 1]);
  ok(isApproxVec(m5, [2, 9, -5, 0, -2, 1, -1, -3, 2]),
    "The mat4.toInverseMat3() function didn't set the values correctly.");
}
