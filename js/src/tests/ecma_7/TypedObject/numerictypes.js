// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 578700;
var summary = 'TypedObjects numeric types';
var actual = '';
var expect = '';

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

function runTests()
{
    printBugNumber(BUGNUMBER);
    printStatus(summary);

    var TestPassCount = 0;
    var TestFailCount = 0;
    var TestTodoCount = 0;

    var TODO = 1;

    function check(fun, todo) {
        var thrown = null;
        var success = false;
        try {
            success = fun();
        } catch (x) {
            thrown = x;
        }

        if (thrown)
            success = false;

        if (todo) {
            TestTodoCount++;

            if (success) {
                var ex = new Error;
                print ("=== TODO but PASSED? ===");
                print (ex.stack);
                print ("========================");
            }

            return;
        }

        if (success) {
            TestPassCount++;
        } else {
            TestFailCount++;

            var ex = new Error;
            print ("=== FAILED ===");
            print (ex.stack);
            if (thrown) {
                print ("    threw exception:");
                print (thrown);
            }
            print ("==============");
        }
    }

    function checkThrows(fun, todo) {
        var thrown = false;
        try {
            fun();
        } catch (x) {
            thrown = true;
        }

        check(() => thrown, todo);
    }

    var types = [uint8, uint16, uint32, int8, int16, int32];
    var strings = ["uint8", "uint16", "uint32", "int8", "int16", "int32"];
    for (var i = 0; i < types.length; i++) {
        var type = types[i];

        check(() => type(true) === 1);
        check(() => type(false) === 0);
        check(() => type(+Infinity) === 0);
        check(() => type(-Infinity) === 0);
        check(() => type(NaN) === 0);
        check(() => type.toSource() === strings[i]);
        check(() => type(null) == 0);
        check(() => type(undefined) == 0);
        check(() => type([]) == 0);
        check(() => type({}) == 0);
        check(() => type(/abcd/) == 0);

        checkThrows(() => new type());
        checkThrows(() => type());
    }

    var floatTypes = [float32, float64];
    var floatStrings = ["float32", "float64"];
    for (var i = 0; i < floatTypes.length; i++) {
        var type = floatTypes[i];

        check(() => type(true) === 1);
        check(() => type(false) === 0);
        check(() => type(+Infinity) === Infinity);
        check(() => type(-Infinity) === -Infinity);
        check(() => Number.isNaN(type(NaN)));
        check(() => type.toSource() === floatStrings[i]);
        check(() => type(null) == 0);
        check(() => Number.isNaN(type(undefined)));
        check(() => type([]) == 0);
        check(() => Number.isNaN(type({})));
        check(() => Number.isNaN(type(/abcd/)));

        checkThrows(() => new type());
        checkThrows(() => type());
    }

    ///// test ranges and creation
    /// uint8
    // valid
    check(() => uint8(0) == 0);
    check(() => uint8(-0) == 0);
    check(() => uint8(129) == 129);
    check(() => uint8(255) == 255);

    // overflow is allowed for explicit conversions
    check(() => uint8(-1) == 255);
    check(() => uint8(-255) == 1);
    check(() => uint8(256) == 0);
    check(() => uint8(2345678) == 206);
    check(() => uint8(3.14) == 3);
    check(() => uint8(342.56) == 86);
    check(() => uint8(-342.56) == 170);

    /// uint8clamped
    // valid
    check(() => uint8Clamped(0) == 0);
    check(() => uint8Clamped(-0) == 0);
    check(() => uint8Clamped(129) == 129);
    check(() => uint8Clamped(-30) == 0);
    check(() => uint8Clamped(254.5) == 254);
    check(() => uint8Clamped(257) == 255);
    check(() => uint8Clamped(513) == 255);
    check(() => uint8Clamped(60000) == 255);

    // strings
    check(() => uint8("0") == 0);
    check(() => uint8("255") == 255);
    check(() => uint8("256") == 0);
    check(() => uint8("0x0f") == 15);
    check(() => uint8("0x00") == 0);
    check(() => uint8("0xff") == 255);
    check(() => uint8("0x1ff") == 255);
    // in JS, string literals with leading zeroes are interpreted as decimal
    check(() => uint8("-0777") == 247);
    check(() => uint8("-0xff") == 0);

    /// uint16
    // valid
    check(() => uint16(65535) == 65535);

    // overflow is allowed for explicit conversions
    check(() => uint16(-1) == 65535);
    check(() => uint16(-65535) == 1);
    check(() => uint16(-65536) == 0);
    check(() => uint16(65536) == 0);

    // strings
    check(() => uint16("0x1234") == 0x1234);
    check(() => uint16("0x00") == 0);
    check(() => uint16("0xffff") == 65535);
    check(() => uint16("-0xffff") == 0);
    check(() => uint16("0xffffff") == 0xffff);

    // wrong types
    check(() => uint16(3.14) == 3); // c-like casts in explicit conversion

    print("done");

    reportCompare(0, TestFailCount, "TypedObjects numeric type tests");
}

runTests();
