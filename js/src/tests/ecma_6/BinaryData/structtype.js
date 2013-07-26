// |reftest| skip-if(!this.hasOwnProperty("Type"))
var BUGNUMBER = 578700;
var summary = 'BinaryData StructType implementation';

function assertThrows(f) {
    var ok = false;
    try {
        f();
    } catch (exc) {
        ok = true;
    }
    if (!ok)
        throw new TypeError("Assertion failed: " + f + " did not throw as expected");
}

function runTests() {
    print(BUGNUMBER + ": " + summary);

    var S = new StructType({x: int32, y: uint8, z: float64});
    assertEq(S.__proto__, StructType.prototype);
    assertEq(S.prototype.__proto__, StructType.prototype.prototype);
    assertEq(S.toString(), "StructType({x: int32, y: uint8, z: float64})");

    assertEq(S.bytes, 13);
    assertEq(S.fields.x, int32);
    assertEq(S.fields.y, uint8);
    assertEq(S.fields.z, float64);

    var s = new S();
    assertEq(s.__proto__, S.prototype);
    s.x = 2;
    s.y = 255;
    s.z = 12.342345;
    assertEq(s.x, 2);
    assertEq(s.y, 255);
    assertEq(s.z, 12.342345);

    var Color = new StructType({r: uint8, g: uint8, b: uint8});
    var white = new Color();
    white.r = white.g = white.b = 255;

    var Car = new StructType({color: Color, weight: uint32});
    assertEq(Car.toString(), "StructType({color: StructType({r: uint8, g: uint8, b: uint8}), weight: uint32})");

    var civic = new Car();
    civic.color = white;
    civic.weight = 1000;

    assertEq(civic.weight, 1000);
    assertEq(civic.color.r, 255);
    assertEq(civic.color.g, 255);
    assertEq(civic.color.b, 255);

    civic.color = {r: 255, g: 0, b: 0};
    assertEq(civic.color.r, 255);
    assertEq(civic.color.g, 0);
    assertEq(civic.color.b, 0);

    assertThrows(function() civic.color = 5);
    assertThrows(function() civic.color = []);
    assertThrows(function() civic.color = {});
    assertThrows(function() civic.color = {r: 2, g: 2});
    civic.color = {r: 2, g: 2, b: "foo"};
    assertEq(civic.color.b, 0);
    assertThrows(function() civic.color = {r: 2, x: 2, y: 255});

    assertThrows(function() civic.transmission = "automatic");

    // Test structural (not reference) equality
    var OtherColor = new StructType({r: uint8, g: uint8, b: uint8});
    var gray = new OtherColor();
    gray.r = gray.g = gray.b = 0xEE;
    civic.color = gray;
    assertEq(civic.color.r, 0xEE);
    assertEq(civic.color.g, 0xEE);
    assertEq(civic.color.b, 0xEE);

    var Showroom = new ArrayType(Car, 10);
    assertEq(Showroom.toString(), "ArrayType(StructType({color: StructType({r: uint8, g: uint8, b: uint8}), weight: uint32}), 10)");
    var mtvHonda = new Showroom();
    mtvHonda[0] = {'color': {'r':0, 'g':255, 'b':255}, 'weight': 1300};

    assertEq(mtvHonda[0].color.g, 255);
    assertEq(mtvHonda[0].weight, 1300);

    if (typeof reportCompare === "function")
        reportCompare(true, true);
    print("Tests complete");
}

runTests();
