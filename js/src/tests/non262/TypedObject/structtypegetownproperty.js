// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 1122552;
var summary = 'Introduce [[GetOwnProperty]] object op';

var StructType = TypedObject.StructType;
var uint8 = TypedObject.uint8;

function runTests() {
    print(BUGNUMBER + ": " + summary);

    var PixelType = new StructType({x: uint8, y: uint8});
    var pixel = new PixelType({x: 15, y: 16});

    var desc = Object.getOwnPropertyDescriptor(pixel, 'x');
    assertEq(typeof desc, "object");
    assertEq(desc.value, 15);
    assertEq(desc.enumerable, true);
    assertEq(desc.writable, true);
    assertEq(desc.configurable, false);

    desc = Object.getOwnPropertyDescriptor(pixel, 'dummy');
    assertEq(typeof desc, "undefined");

    reportCompare(true, true);
    print("Tests complete");
}

runTests();
