/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

if (!this.hasOwnProperty("TypedObject"))
  quit();

setJitCompilerOption("ion.usecount.trigger", 30);

var Vec3u32Type = TypedObject.uint32.array(3);

function foo_u32() {
  for (var i = 0; i < 5000; i += 10) {
    var vec = new Vec3u32Type();
    // making index non-trivially dependent on |i| to foil compiler optimization
    vec[(i)     % 3] = i;
    vec[(i + 1) % 3] = i+1;
    vec[(i + 2) % 3] = i+2;
    var sum = vec[0] + vec[1] + vec[2];
    assertEq(sum, 3*i + 3);
  }
}

foo_u32();
