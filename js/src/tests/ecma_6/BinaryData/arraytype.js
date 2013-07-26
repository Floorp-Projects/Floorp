// |reftest| skip-if(!this.hasOwnProperty("Type"))
var BUGNUMBER = 578700;
var summary = 'BinaryData ArrayType implementation';

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

    assertEq(typeof ArrayType.prototype.prototype.forEach == "function", true);
    assertEq(typeof ArrayType.prototype.prototype.subarray == "function", true);

    assertThrows(function() ArrayType(uint8, 10));
    assertThrows(function() new ArrayType());
    assertThrows(function() new ArrayType(""));
    assertThrows(function() new ArrayType(5));
    assertThrows(function() new ArrayType(uint8, -1));
    var A = new ArrayType(uint8, 10);
    assertEq(A.__proto__, ArrayType.prototype);
    assertEq(A.length, 10);
    assertEq(A.elementType, uint8);
    assertEq(A.bytes, 10);
    assertEq(A.toString(), "ArrayType(uint8, 10)");

    assertEq(A.prototype.__proto__, ArrayType.prototype.prototype);
    assertEq(typeof A.prototype.fill, "function");

    var X = { __proto__: A };
    assertThrows(function() X.repeat(42));

    var a = new A();
    assertEq(a.__proto__, A.prototype);
    assertEq(a.length, 10);

    assertThrows(function() a.length = 2);

    for (var i = 0; i < a.length; i++)
        a[i] = i*2;

    for (var i = 0; i < a.length; i++)
        assertEq(a[i], i*2);

    a.forEach(function(val, i) {
        assertEq(val, i*2);
        assertEq(arguments[2], a);
    });

    // Range.
    assertThrows(function() a[i] = 5);

    assertEq(a[a.length], undefined);

    // constructor takes initial value
    var b = new A(a);
    for (var i = 0; i < a.length; i++)
        assertEq(b[i], i*2);

    var b = new A([0, 1, 0, 1, 0, 1, 0, 1, 0, 1]);
    for (var i = 0; i < b.length; i++)
        assertEq(b[i], i%2);


    assertThrows(function() new A(5));
    assertThrows(function() new A(/fail/));
    // Length different
    assertThrows(function() new A([0, 1, 0, 1, 0, 1, 0, 1, 0]));

    var Vec3 = new ArrayType(float32, 3);
    var Sprite = new ArrayType(Vec3, 3); // say for position, velocity, and direction
    assertEq(Sprite.elementType, Vec3);
    assertEq(Sprite.elementType.elementType, float32);


    var mario = new Sprite();
    // setting using binary data
    mario[0] = new Vec3([1, 0, 0]);
    // setting using JS array conversion
    mario[1] = [1, 1.414, 3.14];

    assertEq(mario[0].length, 3);
    assertEq(mario[0][0], 1);
    assertEq(mario[0][1], 0);
    assertEq(mario[0][2], 0);

    assertThrows(function() mario[1] = 5);
    mario[1][1] = [];
    assertEq(Number.isNaN(mario[1][1]), true);


    // ok this is just for kicks
    var AllSprites = new ArrayType(Sprite, 65536);
    var as = new AllSprites();
    assertEq(as.length, 65536);

    // test methods
    var c = new A();
    c.fill(3);
    for (var i = 0; i < c.length; i++)
        assertEq(c[i], 3);

    assertThrows(function() c.update([3.14, 4.52, 5]));
    //assertThrows(function() c.update([3000, 0, 1, 1, 1, 1, 1, 1, 1, 1]));

    assertThrows(function() Vec3.prototype.fill.call(c, 2));

    var updatingPos = new Vec3();
    updatingPos.update([5, 3, 1]);
    assertEq(updatingPos[0], 5);
    assertEq(updatingPos[1], 3);
    assertEq(updatingPos[2], 1);

    var d = A.repeat(10);
    for (var i = 0; i < d.length; i++)
        assertEq(d[i], 10);

    assertThrows(function() ArrayType.prototype.repeat.call(d, 2));

    var MA = new ArrayType(uint32, 5);
    var ma = new MA([1, 2, 3, 4, 5]);

    var mb = ma.subarray(2);
    assertEq(mb.length, 3);
    assertEq(mb[0], 3);
    assertEq(mb[1], 4);
    assertEq(mb[2], 5);

    assertThrows(function() ma.subarray());
    assertThrows(function() ma.subarray(2.14));
    assertThrows(function() ma.subarray({}));
    assertThrows(function() ma.subarray(2, []));

    // check similarity even though mb's ArrayType
    // is not script accessible
    var Similar = new ArrayType(uint32, 3);
    var sim = new Similar();
    sim.update(mb);
    assertEq(sim[0], 3);
    assertEq(sim[1], 4);
    assertEq(sim[2], 5);

    var range = ma.subarray(0, 3);
    assertEq(range.length, 3);
    assertEq(range[0], 1);
    assertEq(range[1], 2);
    assertEq(range[2], 3);

    assertEq(ma.subarray(ma.length).length, 0);
    assertEq(ma.subarray(ma.length, ma.length-1).length, 0);

    var rangeNeg = ma.subarray(-2);
    assertEq(rangeNeg.length, 2);
    assertEq(rangeNeg[0], 4);
    assertEq(rangeNeg[1], 5);

    var rangeNeg = ma.subarray(-5, -3);
    assertEq(rangeNeg.length, 2);
    assertEq(rangeNeg[0], 1);
    assertEq(rangeNeg[1], 2);

    assertEq(ma.subarray(-2, -3).length, 0);
    assertEq(ma.subarray(-6).length, ma.length);

    var modifyOriginal = ma.subarray(2);
    modifyOriginal[0] = 42;
    assertEq(ma[2], 42);

    ma[4] = 97;
    assertEq(modifyOriginal[2], 97);

    var indexPropDesc = Object.getOwnPropertyDescriptor(as, '0');
    assertEq(typeof indexPropDesc == "undefined", false);
    assertEq(indexPropDesc.configurable, false);
    assertEq(indexPropDesc.enumerable, true);
    assertEq(indexPropDesc.writable, true);


    var lengthPropDesc = Object.getOwnPropertyDescriptor(as, 'length');
    assertEq(typeof lengthPropDesc == "undefined", false);
    assertEq(lengthPropDesc.configurable, false);
    assertEq(lengthPropDesc.enumerable, false);
    assertEq(lengthPropDesc.writable, false);

    assertThrows(function() Object.defineProperty(o, "foo", { value: "bar" }));

    // check if a reference acts the way it should
    var AA = new ArrayType(new ArrayType(uint8, 5), 5);
    var aa = new AA();
    var aa0 = aa[0];
    aa[0] = [0,1,2,3,4];
    for (var i = 0; i < aa0.length; i++)
        assertEq(aa0[i], i);

    if (typeof reportCompare === "function")
        reportCompare(true, true);
    print("Tests complete");
}

runTests();
