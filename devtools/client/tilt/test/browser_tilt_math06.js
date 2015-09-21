/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function test() {
  let q1 = quat4.create();

  ok(q1, "Should have created a quaternion with quat4.create().");
  is(q1.length, 4, "A quat4 should have 4 elements.");

  ok(isApproxVec(q1, [0, 0, 0, 1]),
    "When created, a vec3 should have the values default to identity.");

  quat4.set([1, 2, 3, 4], q1);
  ok(isApproxVec(q1, [1, 2, 3, 4]),
    "The quat4.set() function didn't set the values correctly.");

  quat4.identity(q1);
  ok(isApproxVec(q1, [0, 0, 0, 1]),
    "The quat4.identity() function didn't set the values correctly.");

  quat4.set([5, 6, 7, 8], q1);
  ok(isApproxVec(q1, [5, 6, 7, 8]),
    "The quat4.set() function didn't set the values correctly.");

  quat4.calculateW(q1);
  ok(isApproxVec(q1, [5, 6, 7, -10.440306663513184]),
    "The quat4.calculateW() function didn't compute the values correctly.");

  quat4.inverse(q1);
  ok(isApproxVec(q1, [-5, -6, -7, -10.440306663513184]),
    "The quat4.inverse() function didn't compute the values correctly.");

  quat4.normalize(q1);
  ok(isApproxVec(q1, [
    -0.33786869049072266, -0.40544241666793823,
    -0.4730161726474762, -0.7054905295372009
  ]), "The quat4.normalize() function didn't compute the values correctly.");

  ok(isApprox(quat4.length(q1), 1),
    "The mat4.length() function didn't calculate the value correctly.");
}
