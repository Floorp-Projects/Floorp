/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

if (!this.hasOwnProperty("TypedObject"))
  quit();

var Vec3u32Type = new TypedObject.ArrayType(TypedObject.uint32, 3);

function foo_u32() {
  for (var i = 0; i < 30000; i += 3) {
    var vec = new Vec3u32Type([i, i+1, i+2]);
    var sum = vec[0] + vec[1] + vec[2];
    assertEq(sum, 3*i + 3);
  }
}

foo_u32();
