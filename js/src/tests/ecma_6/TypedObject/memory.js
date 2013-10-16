// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 578700;
var summary = 'TypedObjects memory check';

function spin() {
    for (var i = 0; i < 10000; i++)
        ;
}

var ArrayType = TypedObject.ArrayType;
var StructType = TypedObject.StructType;
var uint8 = TypedObject.uint8;
var uint16 = TypedObject.uint16;
var uint32 = TypedObject.uint32;
var uint8Clamped = TypedObject.uint8Clamped;
var int8 = TypedObject.int8;
var int16 = TypedObject.int16;
var int32 = TypedObject.int32;
var float32 = TypedObject.float32;
var float64 = TypedObject.float64;


function runTests() {
    print(BUGNUMBER + ": " + summary);

    var AA = new ArrayType(new ArrayType(uint8, 5), 5);
    var aa = new AA();
    var aa0 = aa[0];
    aa[0] = [0,1,2,3,4];

    aa = null;

    gc();
    spin();

    for (var i = 0; i < aa0.length; i++)
        assertEq(aa0[i], i);

    var AAA = new ArrayType(AA, 5);
    var aaa = new AAA();
    var a0 = aaa[0][0];

    for (var i = 0; i < a0.length; i++)
        assertEq(a0[i], 0);

    aaa[0] = [[0,1,2,3,4], [0,1,2,3,4], [0,1,2,3,4], [0,1,2,3,4], [0,1,2,3,4]];

    aaa = null;

    gc();
    spin();
    for (var i = 0; i < a0.length; i++)
        assertEq(a0[i], i);

    var Color = new StructType({'r': uint8, 'g': uint8, 'b': uint8});
    var Rainbow = new ArrayType(Color, 7);

    var theOneISawWasJustBlack = Rainbow.repeat({'r': 0, 'g': 0, 'b': 0});

    var middleBand = theOneISawWasJustBlack[3];
    theOneISawWasJustBlack = null;
    gc();
    spin();
    assertEq(middleBand['r'] == 0 && middleBand['g'] == 0 && middleBand['b'] == 0, true);
    middleBand.r = 255;
    middleBand.g = 207;
    middleBand.b = 142;
    assertEq(middleBand['r'] == 255 && middleBand['g'] == 207 && middleBand['b'] == 142, true);

    var scopedType = function() {
        var Point = new StructType({'x': int32, 'y': int32});
        var aPoint = new Point();
        aPoint.x = 4;
        aPoint.y = 5;
        return aPoint;
    }

    var point = scopedType();
    gc();
    spin();
    gc();
    assertEq(point.constructor.fieldTypes.x, int32);
    assertEq(point.constructor.fieldTypes.y, int32);

    if (typeof reportCompare === "function")
        reportCompare(true, true);
    print("Tests complete");
}

runTests();
