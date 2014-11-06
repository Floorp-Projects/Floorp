// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 922115;
var summary = 'TypedObjects ArrayType implementation';

var ArrayType = TypedObject.ArrayType;
var StructType = TypedObject.StructType;
var uint8 = TypedObject.uint8;
var float32 = TypedObject.float32;
var uint32 = TypedObject.uint32;
var ObjectType = TypedObject.Object;

function runTests() {
  (function DimensionLinkedToUndimension() {
    var UintsA = uint32.array();
    var FiveUintsA = UintsA.dimension(5);
    var FiveUintsB = uint32.array(5);

    assertEq(true, FiveUintsA.equivalent(FiveUintsB));
    assertEq(true, FiveUintsA.unsized === UintsA);
    assertEq(true, FiveUintsB.unsized !== UintsA);
  })();

  (function PrototypeHierarchy() {
    var Uint8s = uint8.array();
    assertEq(Uint8s.prototype.__proto__, ArrayType.prototype.prototype);
    Uint8s.prototype.sum = function() {
      var r = 0;
      for (var i = 0; i < this.length; i++)
        r = uint8(r + this[i]);
      return r;
    };

    var FiveUint8s = Uint8s.dimension(5);
    assertEq(FiveUint8s.__proto__, Uint8s);

    var fiveUint8s = new FiveUint8s([128, 128, 128, 128, 128]);
    assertEq(128, fiveUint8s.sum());
  })();

  if (typeof reportCompare === "function")
    reportCompare(true, true);

  print("Tests complete");
}

runTests();

