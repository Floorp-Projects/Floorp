/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function test() {
  let v1 = vec3.create();

  ok(v1, "Should have created a vector with vec3.create().");
  is(v1.length, 3, "A vec3 should have 3 elements.");

  ok(isApproxVec(v1, [0, 0, 0]),
    "When created, a vec3 should have the values default to 0.");

  vec3.set([1, 2, 3], v1);
  ok(isApproxVec(v1, [1, 2, 3]),
    "The vec3.set() function didn't set the values correctly.");

  vec3.zero(v1);
  ok(isApproxVec(v1, [0, 0, 0]),
    "The vec3.zero() function didn't set the values correctly.");

  let v2 = vec3.create([4, 5, 6]);
  ok(isApproxVec(v2, [4, 5, 6]),
    "When cloning arrays, a vec3 should have the values copied.");

  let v3 = vec3.create(v2);
  ok(isApproxVec(v3, [4, 5, 6]),
    "When cloning vectors, a vec3 should have the values copied.");

  vec3.add(v2, v3);
  ok(isApproxVec(v2, [8, 10, 12]),
    "The vec3.add() function didn't set the x value correctly.");

  vec3.subtract(v2, v3);
  ok(isApproxVec(v2, [4, 5, 6]),
    "The vec3.subtract() function didn't set the values correctly.");

  vec3.negate(v2);
  ok(isApproxVec(v2, [-4, -5, -6]),
    "The vec3.negate() function didn't set the values correctly.");

  vec3.scale(v2, -2);
  ok(isApproxVec(v2, [8, 10, 12]),
    "The vec3.scale() function didn't set the values correctly.");

  vec3.normalize(v1);
  ok(isApproxVec(v1, [0, 0, 0]),
    "Normalizing a vector with zero length should return [0, 0, 0].");

  vec3.normalize(v2);
  ok(isApproxVec(v2, [
    0.4558423161506653, 0.5698028802871704, 0.6837634444236755
  ]), "The vec3.normalize() function didn't set the values correctly.");

  vec3.cross(v2, v3);
  ok(isApproxVec(v2, [
    5.960464477539063e-8, -1.1920928955078125e-7, 5.960464477539063e-8
  ]), "The vec3.cross() function didn't set the values correctly.");

  vec3.dot(v2, v3);
  ok(isApproxVec(v2, [
    5.960464477539063e-8, -1.1920928955078125e-7, 5.960464477539063e-8
  ]), "The vec3.dot() function didn't set the values correctly.");

  ok(isApproxVec([vec3.length(v2)], [1.4600096599955427e-7]),
    "The vec3.length() function didn't calculate the value correctly.");

  vec3.direction(v2, v3);
  ok(isApproxVec(v2, [
    -0.4558422863483429, -0.5698028802871704, -0.6837634444236755
  ]), "The vec3.direction() function didn't set the values correctly.");

  vec3.lerp(v2, v3, 0.5);
  ok(isApproxVec(v2, [
    1.7720788717269897, 2.2150986194610596, 2.65811824798584
  ]), "The vec3.lerp() function didn't set the values correctly.");


  vec3.project([100, 100, 10], [0, 0, 100, 100],
    mat4.create(), mat4.perspective(45, 1, 0.1, 100), v1);
  ok(isApproxVec(v1, [-1157.10693359375, 1257.10693359375, 0]),
    "The vec3.project() function didn't set the values correctly.");

  vec3.unproject([100, 100, 1], [0, 0, 100, 100],
    mat4.create(), mat4.perspective(45, 1, 0.1, 100), v1);
  ok(isApproxVec(v1, [
    41.420406341552734, -41.420406341552734, -99.99771118164062
  ]), "The vec3.project() function didn't set the values correctly.");


  let ray = vec3.createRay([10, 10, 0], [100, 100, 1], [0, 0, 100, 100],
    mat4.create(), mat4.perspective(45, 1, 0.1, 100));

  ok(isApproxVec(ray.origin, [
    -0.03313708305358887, 0.03313708305358887, -0.1000000014901161
  ]), "The vec3.createRay() function didn't create the position correctly.");
  ok(isApproxVec(ray.direction, [
    0.35788586614428364, -0.35788586614428364, -0.862458934459091
  ]), "The vec3.createRay() function didn't create the direction correctly.");


  is(vec3.str([0, 0, 0]), "[0, 0, 0]",
    "The vec3.str() function didn't work properly.");
}
