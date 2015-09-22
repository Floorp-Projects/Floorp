/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function test() {
  let m1 = mat3.create();

  ok(m1, "Should have created a matrix with mat3.create().");
  is(m1.length, 9, "A mat3 should have 9 elements.");

  ok(isApproxVec(m1, [1, 0, 0, 0, 1, 0, 0, 0, 1]),
    "When created, a mat3 should have the values default to identity.");

  mat3.set([1, 2, 3, 4, 5, 6, 7, 8, 9], m1);
  ok(isApproxVec(m1, [1, 2, 3, 4, 5, 6, 7, 8, 9]),
    "The mat3.set() function didn't set the values correctly.");

  mat3.transpose(m1);
  ok(isApproxVec(m1, [1, 4, 7, 2, 5, 8, 3, 6, 9]),
    "The mat3.transpose() function didn't set the values correctly.");

  mat3.identity(m1);
  ok(isApproxVec(m1, [1, 0, 0, 0, 1, 0, 0, 0, 1]),
    "The mat3.identity() function didn't set the values correctly.");

  let m2 = mat3.toMat4(m1);
  ok(isApproxVec(m2, [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]),
    "The mat3.toMat4() function didn't set the values correctly.");


  is(mat3.str([1, 2, 3, 4, 5, 6, 7, 8, 9]), "[1, 2, 3, 4, 5, 6, 7, 8, 9]",
    "The mat3.str() function didn't work properly.");
}
