/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

if (!this.hasOwnProperty("TypedObject"))
  quit();

var PointType = new TypedObject.StructType({x: TypedObject.uint16,
                                            y: TypedObject.uint16,
                                            z: TypedObject.uint16});

var VecPointType = new TypedObject.ArrayType(PointType, 3);

var PairVecType = new TypedObject.StructType({fst: VecPointType,
                                              snd: VecPointType});

function foo() {
  for (var i = 0; i < 5000; i += 9) {
    var p = new PairVecType({fst: [{x:    i, y: i+1, z:i+2},
                                   {x:  i+3, y: i+4, z:i+5},
                                   {x:  i+6, y: i+7, z:i+8}],
                             snd: [{x:  i+9, y:i+10, z:i+11},
                                   {x: i+12, y:i+13, z:i+14},
                                   {x: i+15, y:i+16, z:i+17}]
                            });
    var sum;

    sum = p.fst[0].x + p.fst[0].y + p.fst[0].z;
    assertEq(sum, 3*i + 3);
    sum = p.fst[1].x + p.fst[1].y + p.fst[1].z;
    assertEq(sum, 3*i + 12);
    sum = p.fst[2].x + p.fst[2].y + p.fst[2].z;
    assertEq(sum, 3*i + 21);

    sum = p.snd[0].x + p.snd[0].y + p.snd[0].z;
    assertEq(sum, 3*i + 30);
    sum = p.snd[1].x + p.snd[1].y + p.snd[1].z;
    assertEq(sum, 3*i + 39);
    sum = p.snd[2].x + p.snd[2].y + p.snd[2].z;
    assertEq(sum, 3*i + 48);
  }
}

foo();
