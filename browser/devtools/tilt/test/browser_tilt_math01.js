/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function test() {
  ok(isApprox(TiltMath.radians(30), 0.523598775),
    "The radians() function didn't calculate the value correctly.");

  ok(isApprox(TiltMath.degrees(0.5), 28.64788975),
    "The degrees() function didn't calculate the value correctly.");

  ok(isApprox(TiltMath.map(0.5, 0, 1, 0, 100), 50),
    "The map() function didn't calculate the value correctly.");

  is(TiltMath.isPowerOfTwo(32), true,
    "The isPowerOfTwo() function didn't return the expected value.");

  is(TiltMath.isPowerOfTwo(33), false,
    "The isPowerOfTwo() function didn't return the expected value.");

  ok(isApprox(TiltMath.nextPowerOfTwo(31), 32),
    "The nextPowerOfTwo() function didn't calculate the 1st value correctly.");

  ok(isApprox(TiltMath.nextPowerOfTwo(32), 32),
    "The nextPowerOfTwo() function didn't calculate the 2nd value correctly.");

  ok(isApprox(TiltMath.nextPowerOfTwo(33), 64),
    "The nextPowerOfTwo() function didn't calculate the 3rd value correctly.");

  ok(isApprox(TiltMath.clamp(5, 1, 3), 3),
    "The clamp() function didn't calculate the 1st value correctly.");

  ok(isApprox(TiltMath.clamp(5, 3, 1), 3),
    "The clamp() function didn't calculate the 2nd value correctly.");

  ok(isApprox(TiltMath.saturate(5), 1),
    "The saturate() function didn't calculate the 1st value correctly.");

  ok(isApprox(TiltMath.saturate(-5), 0),
    "The saturate() function didn't calculate the 2nd value correctly.");

  ok(isApproxVec(TiltMath.hex2rgba("#f00"), [1, 0, 0, 1]),
    "The hex2rgba() function didn't calculate the 1st rgba values correctly.");

  ok(isApproxVec(TiltMath.hex2rgba("#f008"), [1, 0, 0, 0.53]),
    "The hex2rgba() function didn't calculate the 2nd rgba values correctly.");

  ok(isApproxVec(TiltMath.hex2rgba("#ff0000"), [1, 0, 0, 1]),
    "The hex2rgba() function didn't calculate the 3rd rgba values correctly.");

  ok(isApproxVec(TiltMath.hex2rgba("#ff0000aa"), [1, 0, 0, 0.66]),
    "The hex2rgba() function didn't calculate the 4th rgba values correctly.");

  ok(isApproxVec(TiltMath.hex2rgba("rgba(255, 0, 0, 0.5)"), [1, 0, 0, 0.5]),
    "The hex2rgba() function didn't calculate the 5th rgba values correctly.");

  ok(isApproxVec(TiltMath.hex2rgba("rgb(255, 0, 0)"), [1, 0, 0, 1]),
    "The hex2rgba() function didn't calculate the 6th rgba values correctly.");
}
