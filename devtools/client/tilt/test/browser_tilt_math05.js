/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function test() {
  let m1 = mat4.create([
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]);

  let m2 = mat4.create([
    0, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15, 4, 8, 12, 16]);

  mat4.multiply(m1, m2);
  ok(isApproxVec(m1, [
    275, 302, 329, 356, 304, 336, 368, 400,
    332, 368, 404, 440, 360, 400, 440, 480
  ]), "The mat4.multiply() function didn't set the values correctly.");

  let v1 = mat4.multiplyVec3(m1, [1, 2, 3]);
  ok(isApproxVec(v1, [2239, 2478, 2717]),
    "The mat4.multiplyVec3() function didn't set the values correctly.");

  let v2 = mat4.multiplyVec4(m1, [1, 2, 3, 0]);
  ok(isApproxVec(v2, [1879, 2078, 2277, 2476]),
    "The mat4.multiplyVec4() function didn't set the values correctly.");

  mat4.translate(m1, [1, 2, 3]);
  ok(isApproxVec(m1, [
    275, 302, 329, 356, 304, 336, 368, 400,
    332, 368, 404, 440, 2239, 2478, 2717, 2956
  ]), "The mat4.translate() function didn't set the values correctly.");

  mat4.scale(m1, [1, 2, 3]);
  ok(isApproxVec(m1, [
    275, 302, 329, 356, 608, 672, 736, 800,
    996, 1104, 1212, 1320, 2239, 2478, 2717, 2956
  ]), "The mat4.scale() function didn't set the values correctly.");

  mat4.rotate(m1, 0.5, [1, 1, 1]);
  ok(isApproxVec(m1, [
    210.6123046875, 230.2483367919922, 249.88438415527344, 269.5204162597656,
    809.8145751953125, 896.520751953125, 983.2268676757812,
    1069.9329833984375, 858.5731201171875, 951.23095703125,
    1043.8887939453125, 1136.5465087890625, 2239, 2478, 2717, 2956
  ]), "The mat4.rotate() function didn't set the values correctly.");

  mat4.rotateX(m1, 0.5);
  ok(isApproxVec(m1, [
    210.6123046875, 230.2483367919922, 249.88438415527344, 269.5204162597656,
    1122.301025390625, 1242.8154296875, 1363.3297119140625,
    1483.843994140625, 365.2230224609375, 404.96875, 444.71453857421875,
    484.460205078125, 2239, 2478, 2717, 2956
  ]), "The mat4.rotateX() function didn't set the values correctly.");

  mat4.rotateY(m1, 0.5);
  ok(isApproxVec(m1, [
    9.732441902160645, 7.909564018249512, 6.086670875549316,
    4.263822555541992, 1122.301025390625, 1242.8154296875, 1363.3297119140625,
    1483.843994140625, 421.48626708984375, 465.78045654296875,
    510.0746765136719, 554.3687744140625, 2239, 2478, 2717, 2956
  ]), "The mat4.rotateY() function didn't set the values correctly.");

  mat4.rotateZ(m1, 0.5);
  ok(isApproxVec(m1, [
    546.6007690429688, 602.7787475585938, 658.9566650390625, 715.1345825195312,
    980.245849609375, 1086.881103515625, 1193.5162353515625,
    1300.1514892578125, 421.48626708984375, 465.78045654296875,
    510.0746765136719, 554.3687744140625, 2239, 2478, 2717, 2956
  ]), "The mat4.rotateZ() function didn't set the values correctly.");


  let m3 = mat4.frustum(0, 100, 200, 0, 0.1, 100);
  ok(isApproxVec(m3, [
    0.0020000000949949026, 0, 0, 0, 0, -0.0010000000474974513, 0, 0, 1, -1,
    -1.0020020008087158, -1, 0, 0, -0.20020020008087158, 0
  ]), "The mat4.frustum() function didn't compute the values correctly.");

  let m4 = mat4.perspective(45, 1.6, 0.1, 100);
  ok(isApproxVec(m4, [1.5088834762573242, 0, 0, 0, 0, 2.4142136573791504, 0,
    0, 0, 0, -1.0020020008087158, -1, 0, 0, -0.20020020008087158, 0
  ]), "The mat4.frustum() function didn't compute the values correctly.");

  let m5 = mat4.ortho(0, 100, 200, 0, -1, 1);
  ok(isApproxVec(m5, [
    0.019999999552965164, 0, 0, 0, 0, -0.009999999776482582, 0, 0,
    0, 0, -1, 0, -1, 1, 0, 1
  ]), "The mat4.ortho() function didn't compute the values correctly.");

  let m6 = mat4.lookAt([1, 2, 3], [4, 5, 6], [0, 1, 0]);
  ok(isApproxVec(m6, [
    -0.7071067690849304, -0.40824830532073975, -0.5773502588272095, 0, 0,
    0.8164966106414795, -0.5773502588272095, 0, 0.7071067690849304,
    -0.40824830532073975, -0.5773502588272095, 0, -1.4142135381698608, 0,
    3.464101552963257, 1
  ]), "The mat4.lookAt() function didn't compute the values correctly.");


  is(mat4.str([
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]),
  "[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]",
  "The mat4.str() function didn't work properly.");
}
