/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function test() {
  let q1 = quat4.create([1, 2, 3, 4]);
  let q2 = quat4.create([5, 6, 7, 8]);

  quat4.multiply(q1, q2);
  ok(isApproxVec(q1, [24, 48, 48, -6]),
    "The quat4.multiply() function didn't set the values correctly.");

  let v1 = quat4.multiplyVec3(q1, [9, 9, 9]);
  ok(isApproxVec(v1, [5508, 54756, 59940]),
    "The quat4.multiplyVec3() function didn't set the values correctly.");

  let m1 = quat4.toMat3(q1);
  ok(isApproxVec(m1, [
    -9215, 2880, 1728, 1728, -5759, 4896, 2880, 4320, -5759
  ]), "The quat4.toMat3() function didn't set the values correctly.");

  let m2 = quat4.toMat4(q1);
  ok(isApproxVec(m2, [
    -9215, 2880, 1728, 0, 1728, -5759, 4896, 0,
    2880, 4320, -5759, 0, 0, 0, 0, 1
  ]), "The quat4.toMat4() function didn't set the values correctly.");

  quat4.calculateW(q1);
  quat4.calculateW(q2);
  quat4.slerp(q1, q2, 0.5);
  ok(isApproxVec(q1, [24, 48, 48, -71.99305725097656]),
    "The quat4.slerp() function didn't set the values correctly.");

  let q3 = quat4.fromAxis([1, 1, 1], 0.5);
  ok(isApproxVec(q3, [
    0.24740396440029144, 0.24740396440029144, 0.24740396440029144,
    0.9689124226570129
  ]), "The quat4.fromAxis() function didn't compute the values correctly.");

  let q4 = quat4.fromEuler(0.5, 0.75, 1.25);
  ok(isApproxVec(q4, [
    0.15310347080230713, 0.39433568716049194,
    0.4540249705314636, 0.7841683626174927
  ]), "The quat4.fromEuler() function didn't compute the values correctly.");


  is(quat4.str([1, 2, 3, 4]), "[1, 2, 3, 4]",
    "The quat4.str() function didn't work properly.");
}
