/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

if (!this.hasOwnProperty("TypedObject") || !this.hasOwnProperty("SIMD"))
  quit();

var Float32x4 = SIMD.Float32x4;
Float32x4.array(1);
