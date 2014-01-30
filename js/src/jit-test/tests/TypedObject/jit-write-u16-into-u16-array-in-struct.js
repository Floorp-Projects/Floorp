/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

if (!this.hasOwnProperty("TypedObject"))
  quit();

setJitCompilerOption("ion.usecount.trigger", 30);

var Vec3u16Type = TypedObject.uint16.array(3);
var PairVec3u16Type = new TypedObject.StructType({fst: Vec3u16Type,
                                                  snd: Vec3u16Type});

function foo_u16() {
  for (var i = 0; i < 5000; i += 10) {
    var p = new PairVec3u16Type();

    p.fst[(i)   % 3] = i;
    p.fst[(i+1) % 3] = i+1;
    p.fst[(i+2) % 3] = i+2;

    p.snd[(i)   % 3] = i+3;
    p.snd[(i+1) % 3] = i+4;
    p.snd[(i+2) % 3] = i+5;

    var sum = p.fst[0] + p.fst[1] + p.fst[2];
    assertEq(sum, 3*i + 3);
    sum = p.snd[0] + p.snd[1] + p.snd[2];
    assertEq(sum, 3*i + 12);
  }
}

foo_u16();
