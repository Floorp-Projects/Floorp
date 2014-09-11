/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

if (!this.hasOwnProperty("TypedObject"))
  quit();

setJitCompilerOption("ion.warmup.trigger", 30);

var PointType = TypedObject.uint32.array(3);
var VecPointType = PointType.array(3);

function foo() {
  for (var i = 0; i < 5000; i += 10) {
    var vec = new VecPointType();

    var i0 = i % 3;
    var i1 = (i+1) % 3;
    var i2 = (i+2) % 3;

    vec[i0][i0] = i;
    vec[i0][i1] = i+1;
    vec[i0][i2] = i+2;

    vec[i1][i0] = i+3;
    vec[i1][i1] = i+4;
    vec[i1][i2] = i+5;

    vec[i2][i0] = i+6;
    vec[i2][i1] = i+7;
    vec[i2][i2] = i+8;

    var sum = vec[i0][i0] + vec[i0][i1] + vec[i0][i2];
    assertEq(sum, 3*i + 3);
    sum = vec[i1][i0] + vec[i1][i1] + vec[i1][i2];
    assertEq(sum, 3*i + 12);
    sum = vec[i2][i0] + vec[i2][i1] + vec[i2][i2];
    assertEq(sum, 3*i + 21);
  }
}

foo();
