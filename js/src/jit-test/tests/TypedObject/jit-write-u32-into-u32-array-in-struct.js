/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

if (!this.hasOwnProperty("TypedObject"))
  quit();

setJitCompilerOption("ion.warmup.trigger", 30);

var Vec3u32Type = TypedObject.uint32.array(3);
var PairVec3u32Type = new TypedObject.StructType({fst: Vec3u32Type,
                                                  snd: Vec3u32Type});

function foo_u32() {
  for (var i = 0; i < 5000; i += 10) {
    var p = new PairVec3u32Type();

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

foo_u32();
