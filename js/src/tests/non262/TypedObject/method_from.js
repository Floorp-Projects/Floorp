// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 939715;
var summary = 'method type.from';

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

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

// Test name format:

// from<N>DimArrayOf<G1>sTo<G2>s where <N> is a positive integer (or its
// equivalent word in English) and <G1> and <G2> are both grain types
// (potentially an array themselves.)

function fromOneDimArrayOfUint8ToUint32s() {
  var intype = uint8.array(4);
  var type = uint32.array(4);
  var i1 = intype.build(i => i);
  var r1 = type.from(i1, j => j*2);
  var r2 = type.from(i1, 1, j => j*2);
  assertTypedEqual(type, r1, new type([0, 2, 4, 6]));
  assertTypedEqual(type, r1, r2);
}

function fromOneDimArrayOfUint32ToUint8s() {
  var intype = uint32.array(4);
  var type = uint8.array(4);
  var i1 = intype.build(i => i);
  var r1 = type.from(i1, j => j*200);
  var r2 = type.from(i1, 1, j => j*200);
  assertTypedEqual(type, r1, new type([0, 200, 400 % 256, 600 % 256]));
  assertTypedEqual(type, r1, r2);
}

function fromTwoDimArrayOfUint8ToUint32s() {
  var intype = uint8.array(4).array(4);
  var rowtype = uint32.array(4);
  var type = rowtype.array(4);
  var i1 = new type([[10, 11, 12, 13],
                     [20, 21, 22, 23],
                     [30, 31, 32, 33],
                     [40, 41, 42, 43]]);

  var r1 = type.from(i1, 2, x => x*2);
  var r2 = type.from(i1, 1, a => rowtype.from(a, 1, x => x*2));
  var r3 = type.from(i1, 1, (a, j, c, out) => { out[0] = a[0]*2;
                                                out[1] = a[1]*2;
                                                out[2] = a[2]*2;
                                                out[3] = a[3]*2; });
  assertTypedEqual(type, r1, new type([[20, 22, 24, 26],
                                       [40, 42, 44, 46],
                                       [60, 62, 64, 66],
                                       [80, 82, 84, 86]]));
  assertTypedEqual(type, r1, r2);
  assertTypedEqual(type, r1, r3);
}

function fromTwoDimArrayOfUint32ToUint8s() {
  var intype = uint32.array(4).array(4);
  var rowtype = uint8.array(4);
  var type = rowtype.array(4);
  var i1 = new type([[10, 11, 12, 13],
                     [20, 21, 22, 23],
                     [30, 31, 32, 33],
                     [40, 41, 42, 43]]);

  var r1 = type.from(i1, 2, x => x*2);
  var r2 = type.from(i1, 1, a => rowtype.from(a, 1, x => x*2));
  var r3 = type.from(i1, 1, (a, j, c, out) => { out[0] = a[0]*2;
                                                out[1] = a[1]*2;
                                                out[2] = a[2]*2;
                                                out[3] = a[3]*2; });
  assertTypedEqual(type, r1, new type([[20, 22, 24, 26],
                                       [40, 42, 44, 46],
                                       [60, 62, 64, 66],
                                       [80, 82, 84, 86]]));
  assertTypedEqual(type, r1, r2);
  assertTypedEqual(type, r1, r3);
}

function fromOneDimArrayOfArrayOfUint8ToUint32s() {
  var intype = uint8.array(4).array(4);
  var type = uint32.array(4);
  var i1 = new intype([[0xdd, 0xcc, 0xbb, 0xaa],
                       [0x09, 0x08, 0x07, 0x06],
                       [0x15, 0x14, 0x13, 0x12],
                       [0x23, 0x32, 0x41, 0x50]]);

  function combine(a,b,c,d) { return a << 24 | b << 16 | c << 8 | d; }

  var r1 = type.from(i1, x => combine(x[0], x[1], x[2], x[3]));
  assertTypedEqual(type, r1, new type([0xddccbbaa,
                                       0x09080706,
                                       0x15141312,
                                       0x23324150]));
}

function fromOneDimArrayOfUint32ToArrayOfUint8s() {
  var intype = uint32.array(4);
  var type = uint8.array(4).array(4);
  var i1 = new intype([0xddccbbaa,
                       0x09080706,
                       0x15141312,
                       0x23324150]);

  function divide(a) { return [a >> 24 & 0xFF, a >> 16 & 0xFF, a >> 8 & 0xFF, a & 0xFF]; }

  var r1 = type.from(i1, x => divide(x));
  var r2 = type.from(i1, 1, (x, i, c, out) => {
                          var [a,b,c,d] = divide(x);
                          out[0] = a; out[1] = b; out[2] = c; out[3] = d;
                        });
  assertTypedEqual(type, r1, new type([[0xdd, 0xcc, 0xbb, 0xaa],
                                       [0x09, 0x08, 0x07, 0x06],
                                       [0x15, 0x14, 0x13, 0x12],
                                       [0x23, 0x32, 0x41, 0x50]]));
  assertTypedEqual(type, r1, r2);
}

var Grain = new StructType({f: uint32});
function wrapG(v) { return new Grain({f: v}); }
function doubleG(g) { return new Grain({f: g.f * 2}); }
function tenG(x, y) { return new Grain({f: x * 10 + y}); }

function fromOneDimArrayOfStructsToStructs() {
  var type = Grain.array(4);
  var i1 = type.build(wrapG);
  var r1 = type.from(i1, doubleG);
  var r2 = type.from(i1, 1, doubleG);
  var r3 = type.from(i1, 1, (g, j, c, out) => { out.f = g.f * 2; });
  assertTypedEqual(type, r1, new type([{f:0}, {f:2},
                                       {f:4}, {f:6}]));
  assertTypedEqual(type, r1, r2);
  assertTypedEqual(type, r1, r3);
}

function fromTwoDimArrayOfStructsToStructs() {
  var rowtype = Grain.array(2);
  var type = rowtype.array(2);
  var i1 = type.build(2, tenG);
  var r1 = type.from(i1, 2, doubleG);
  var r2 = type.from(i1, 1, (m) => rowtype.from(m, 1, doubleG));
  var r3 = type.from(i1, 1,
    (m, j, c, out) => { out[0].f = m[0].f * 2; out[1].f = m[1].f * 2; });
  assertTypedEqual(type, r1, new type([[{f:00}, {f:02}],
                                       [{f:20}, {f:22}]]));
  assertTypedEqual(type, r1, r2);
  assertTypedEqual(type, r1, r3);
}

function fromOneDimArrayOfStructsToArrayOfStructs() {
  var Line = Grain.array(2);
  var Box = Line.array(2);
  var i1 = Line.build(wrapG);
  var r1 = Box.from(i1, (g) => Line.build((y) => tenG(g.f, y)));
  var r2 = Box.from(i1, (g) => Line.from(i1, (y) => tenG(g.f, y.f)));
  var r3 = Box.from(i1,
    (g, j, c, out) => { out[0] = tenG(g.f, 0); out[1] = tenG(g.f, 1); });
  assertTypedEqual(Box, r1, new Box([[{f:00}, {f:01}],
                                     [{f:10}, {f:11}]]));
  assertTypedEqual(Box, r1, r2);
  assertTypedEqual(Box, r1, r3);
}

function Array_build(n, f) {
  var a = new Array(n);
  for ( var i=0 ; i < n ; i++ )
    a[i] = f(i);
  return a;
}

function fromUntypedArrayToUint32s() {
  var type = uint32.array(4);
  var i1 = Array_build(4, i => i);
  var r1 = type.from(i1, j => j*2);
  var r2 = type.from(i1, 1, j => j*2);
  assertTypedEqual(type, r1, new type([0, 2, 4, 6]));
  assertTypedEqual(type, r1, r2);
}

function fromUntypedArrayToUint8s() {
  var type = uint8.array(4);
  var i1 = Array_build(4, i => i);
  var r1 = type.from(i1, j => j*200);
  var r2 = type.from(i1, 1, j => j*200);
  assertTypedEqual(type, r1, new type([0, 200, 400 % 256, 600 % 256]));
  assertTypedEqual(type, r1, r2);
}

function fromNonArrayTypedObjects() {
    var type = TypedObject.uint32.array(4);
    var myStruct = new StructType({x: uint32});
    var r1 = type.from(new myStruct({x: 42}), j => j);
    assertTypedEqual(type, r1, new type([0,0,0,0]));

    var r2 = type.from(SIMD.Int32x4(0,0,0,0), j => j);
    assertTypedEqual(type, r1, new type([0,0,0,0]));
}

function runTests() {
    print(BUGNUMBER + ": " + summary);

    fromOneDimArrayOfUint8ToUint32s();
    fromOneDimArrayOfUint32ToUint8s();

    fromTwoDimArrayOfUint8ToUint32s();
    fromTwoDimArrayOfUint32ToUint8s();

    fromOneDimArrayOfArrayOfUint8ToUint32s();
    fromOneDimArrayOfUint32ToArrayOfUint8s();

    fromOneDimArrayOfStructsToStructs();
    fromTwoDimArrayOfStructsToStructs();

    fromOneDimArrayOfStructsToArrayOfStructs();

    fromUntypedArrayToUint32s();
    fromUntypedArrayToUint8s();

    if (typeof reportCompare === "function")
        reportCompare(true, true);
    print("Tests complete");
}

runTests();
