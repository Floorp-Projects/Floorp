// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 922115;
var summary = 'TypedObjects ArrayType implementation';

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var { ArrayType, StructType, uint8, float32, uint32 } = TypedObject;
var ObjectType = TypedObject.Object;

function runTests() {
  print(BUGNUMBER + ": " + summary);

  (function SimpleArrayOfTwoObjects() {
    print("SimpleArrayOfTwoObjects");
    var Objects = new ArrayType(ObjectType);
    var objects2 = new Objects([{f: "Hello"},
                                {f: "World"}]);
    assertEq(objects2[0].f, "Hello");
    assertEq(objects2[1].f, "World");
    assertEq(objects2.length, 2);
  })();

  (function EmbedUnsizedArraysBad() {
    print("EmbedUnsizedArraysBad");
    var Objects = new ArrayType(ObjectType);
    assertThrows(() => new ArrayType(Objects));
    assertThrows(() => new StructType({f: Objects}));
  })();

  (function MultipleSizes() {
    print("MultipleSizes");
    var Uints = new ArrayType(uint32);
    var Point = new StructType({values: new ArrayType(uint32).dimension(3)});

    var uints = new Uints([0, 1, 2]);
    var point = new Point({values: uints});

    assertEq(uints.length, point.values.length);
    for (var i = 0; i < uints.length; i++) {
      assertEq(uints[i], i);
      assertEq(uints[i], point.values[i]);
    }
  })();

  if (typeof reportCompare === "function")
    reportCompare(true, true);
  print("Tests complete");
}

runTests();

