/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

if (!this.hasOwnProperty("TypedObject"))
  quit();

var PointType = new TypedObject.StructType({x: TypedObject.uint32,
                                            y: TypedObject.uint32,
                                            z: TypedObject.uint32});

var VecPointType = PointType.array(3);

function foo() {
  for (var i = 0; i < 10000; i += 9) {
    var vec = new VecPointType([
      {x: i,   y:i+1, z:i+2},
      {x: i+3, y:i+4, z:i+5},
      {x: i+6, y:i+7, z:i+8}]);
    var sum = vec[0].x + vec[0].y + vec[0].z;
    assertEq(sum, 3*i + 3);
    sum = vec[1].x + vec[1].y + vec[1].z;
    assertEq(sum, 3*i + 12);
    sum = vec[2].x + vec[2].y + vec[2].z;
    assertEq(sum, 3*i + 21);
  }
}

foo();
