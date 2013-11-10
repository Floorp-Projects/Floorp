/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

if (!this.hasOwnProperty("TypedObject"))
  quit();

var Vec3u32Type = new TypedObject.ArrayType(TypedObject.uint32, 3);
var PairVec3u32Type = new TypedObject.StructType({fst: Vec3u32Type,
                                                  snd: Vec3u32Type});

function foo_u32() {
  for (var i = 0; i < 15000; i += 6) {
    var p = new PairVec3u32Type({fst: [i, i+1, i+2],
                                 snd: [i+3,i+4,i+5]});
    var sum = p.fst[0] + p.fst[1] + p.fst[2];
    assertEq(sum, 3*i + 3);
    sum = p.snd[0] + p.snd[1] + p.snd[2];
    assertEq(sum, 3*i + 12);
  }
}

foo_u32();
