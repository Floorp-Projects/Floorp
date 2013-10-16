if (!this.hasOwnProperty("TypedObject"))
  quit();

var PointType = new TypedObject.StructType({x: TypedObject.uint32,
                                            y: TypedObject.uint32,
                                            z: TypedObject.uint32});

function foo() {
  for (var i = 0; i < 30000; i += 3) {
    var pt = new PointType({x: i, y: i+1, z: i+2});
    var sum = pt.x + pt.y + pt.z;
    assertEq(sum, 3*i + 3);
  }
}

foo();
