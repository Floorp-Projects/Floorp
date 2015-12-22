// |reftest| skip-if(!this.hasOwnProperty("SIMD"))
var Float32x4 = SIMD.Float32x4;
var Int8x16 = SIMD.Int8x16;
var Int16x8 = SIMD.Int16x8;
var Int32x4 = SIMD.Int32x4;
var Bool8x16 = SIMD.Bool8x16;
var Bool16x8 = SIMD.Bool16x8;
var Bool32x4 = SIMD.Bool32x4;
var Bool64x2 = SIMD.Bool64x2;

function testFloat32x4add() {
  function addf(a, b) {
    return Math.fround(Math.fround(a) + Math.fround(b));
  }

  var vals = [
    [[1, 2, 3, 4], [10, 20, 30, 40]],
    [[1.57, 2.27, 3.57, 4.19], [10.31, 20.49, 30.41, 40.72]],
    [[NaN, -0, Infinity, -Infinity], [0, -0, -Infinity, -Infinity]]
  ];

  for (var [v,w] of vals) {
    testBinaryFunc(Float32x4(...v), Float32x4(...w), Float32x4.add, addf);
  }
}

function testFloat32x4and() {
  var andf = (function() {
    var i = new Int32Array(3);
    var f = new Float32Array(i.buffer);
    return function(x, y) {
      f[0] = x;
      f[1] = y;
      i[2] = i[0] & i[1];
      return f[2];
    };
  })();

  var vals = [
    [[1, 2, 3, 4], [10, 20, 30, 40]],
    [[1.51, 2.98, 3.65, 4.34], [10.29, 20.12, 30.79, 40.41]],
    [[NaN, -0, Infinity, -Infinity], [NaN, -0, -Infinity, Infinity]]
  ];

  for (var [v,w] of vals) {
    testBinaryFunc(Float32x4(...v), Float32x4(...w), Float32x4.and, andf);
  }
}

function testFloat32x4div() {
  function divf(a, b) {
    return Math.fround(Math.fround(a) / Math.fround(b));
  }

  var vals = [
    [[1, 2, 3, 4], [10, 20, 30, 40]],
    [[1.26, 2.03, 3.17, 4.59], [11.025, 17.3768, 29.1957, 46.4049]],
    [[0, -0, Infinity, -Infinity], [1, 1, -Infinity, Infinity]]
  ];

  for (var [v,w] of vals) {
    testBinaryFunc(Float32x4(...v), Float32x4(...w), Float32x4.div, divf);
  }
}

function testFloat32x4mul() {
  function mulf(a, b) {
    return Math.fround(Math.fround(a) * Math.fround(b));
  }

  var vals = [
    [[1, 2, 3, 4], [10, 20, 30, 40]],
    [[1.66, 2.57, 3.73, 4.12], [10.67, 20.68, 30.02, 40.58]],
    [[NaN, -0, Infinity, -Infinity], [NaN, -0, -Infinity, 0]]
  ];

  for (var [v,w] of vals) {
    testBinaryFunc(Float32x4(...v), Float32x4(...w), Float32x4.mul, mulf);
  }
}

function testFloat32x4or() {
  var orf = (function() {
    var i = new Int32Array(3);
    var f = new Float32Array(i.buffer);
    return function(x, y) {
      f[0] = x;
      f[1] = y;
      i[2] = i[0] | i[1];
      return f[2];
    };
  })();

  var vals = [
    [[1, 2, 3, 4], [10, 20, 30, 40]],
    [[1.12, 2.39, 3.83, 4.57], [10.76, 20.41, 30.96, 40.23]],
    [[NaN, -0, Infinity, -Infinity], [5, 5, -Infinity, Infinity]]
  ];

  for (var [v,w] of vals) {
    testBinaryFunc(Float32x4(...v), Float32x4(...w), Float32x4.or, orf);
  }
}

function testFloat32x4sub() {
  function subf(a, b) {
    return Math.fround(Math.fround(a) - Math.fround(b));
  }

  var vals = [
    [[1, 2, 3, 4], [10, 20, 30, 40]],
    [[1.34, 2.95, 3.17, 4.29], [10.18, 20.43, 30.63, 40.38]],
    [[NaN, -0, -Infinity, -Infinity], [NaN, -0, Infinity, -Infinity]]
  ];

  for (var [v,w] of vals) {
    testBinaryFunc(Float32x4(...v), Float32x4(...w), Float32x4.sub, subf);
  }
}

function testFloat32x4xor() {
  var xorf = (function() {
    var i = new Int32Array(3);
    var f = new Float32Array(i.buffer);
    return function(x, y) {
      f[0] = x;
      f[1] = y;
      i[2] = i[0] ^ i[1];
      return f[2];
    };
  })();

  var vals = [
    [[1, 2, 3, 4], [10, 20, 30, 40]],
    [[1.07, 2.62, 3.79, 4.15], [10.38, 20.47, 30.44, 40.16]],
    [[NaN, -0, Infinity, -Infinity], [-0, Infinity, -Infinity, NaN]]
  ];

  for (var [v,w] of vals) {
    testBinaryFunc(Float32x4(...v), Float32x4(...w), Float32x4.xor, xorf);
  }
}

var i8x16vals = [
  [[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16],
   [10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160]],
  [[INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN, INT8_MAX, INT8_MIN, -2, -3, -4, -5, -6, -7, -8, -9],
   [1, 1, -1, -1, INT8_MAX, INT8_MAX, INT8_MIN, INT8_MIN, 8, 9, 10, 11, 12, 13, 14, 15]]
];

function testInt8x16add() {
  function addi(a, b) {
    return (a + b) << 24 >> 24;
  }

  for (var [v,w] of i8x16vals) {
    testBinaryFunc(Int8x16(...v), Int8x16(...w), Int8x16.add, addi);
  }
}

function testInt8x16and() {
  function andi(a, b) {
    return (a & b) << 24 >> 24;
  }

  for (var [v,w] of i8x16vals) {
    testBinaryFunc(Int8x16(...v), Int8x16(...w), Int8x16.and, andi);
  }
}

function testInt8x16mul() {
  function muli(x, y) {
    return (x * y) << 24 >> 24;
  }

  for (var [v,w] of i8x16vals) {
    testBinaryFunc(Int8x16(...v), Int8x16(...w), Int8x16.mul, muli);
  }
}

function testInt8x16or() {
  function ori(a, b) {
    return (a | b) << 24 >> 24;
  }

  for (var [v,w] of i8x16vals) {
    testBinaryFunc(Int8x16(...v), Int8x16(...w), Int8x16.or, ori);
  }
}

function testInt8x16sub() {
  function subi(a, b) {
    return (a - b) << 24 >> 24;
  }

  for (var [v,w] of i8x16vals) {
    testBinaryFunc(Int8x16(...v), Int8x16(...w), Int8x16.sub, subi);
  }
}

function testInt8x16xor() {
  function xori(a, b) {
    return (a ^ b) << 24 >> 24;
  }

  for (var [v,w] of i8x16vals) {
    testBinaryFunc(Int8x16(...v), Int8x16(...w), Int8x16.xor, xori);
  }
}

var i16x8vals = [
  [[1, 2, 3, 4, 5, 6, 7, 8],
   [10, 20, 30, 40, 50, 60, 70, 80]],
  [[INT16_MAX, INT16_MIN, INT16_MAX, INT16_MIN, INT16_MAX, INT16_MIN, INT16_MAX, INT16_MIN],
   [1, 1, -1, -1, INT16_MAX, INT16_MAX, INT16_MIN, INT16_MIN]]
];

function testInt16x8add() {
  function addi(a, b) {
    return (a + b) << 16 >> 16;
  }

  for (var [v,w] of i16x8vals) {
    testBinaryFunc(Int16x8(...v), Int16x8(...w), Int16x8.add, addi);
  }
}

function testInt16x8and() {
  function andi(a, b) {
    return (a & b) << 16 >> 16;
  }

  for (var [v,w] of i16x8vals) {
    testBinaryFunc(Int16x8(...v), Int16x8(...w), Int16x8.and, andi);
  }
}

function testInt16x8mul() {
  function muli(x, y) {
    return (x * y) << 16 >> 16;
  }

  for (var [v,w] of i16x8vals) {
    testBinaryFunc(Int16x8(...v), Int16x8(...w), Int16x8.mul, muli);
  }
}

function testInt16x8or() {
  function ori(a, b) {
    return (a | b) << 16 >> 16;
  }

  for (var [v,w] of i16x8vals) {
    testBinaryFunc(Int16x8(...v), Int16x8(...w), Int16x8.or, ori);
  }
}

function testInt16x8sub() {
  function subi(a, b) {
    return (a - b) << 16 >> 16;
  }

  for (var [v,w] of i16x8vals) {
    testBinaryFunc(Int16x8(...v), Int16x8(...w), Int16x8.sub, subi);
  }
}

function testInt16x8xor() {
  function xori(a, b) {
    return (a ^ b) << 16 >> 16;
  }

  for (var [v,w] of i16x8vals) {
    testBinaryFunc(Int16x8(...v), Int16x8(...w), Int16x8.xor, xori);
  }
}

function testInt32x4add() {
  function addi(a, b) {
    return (a + b) | 0;
  }

  var valsExp = [
    [[1, 2, 3, 4], [10, 20, 30, 40]],
    [[INT32_MAX, INT32_MIN, INT32_MAX, INT32_MIN], [1, -1, 0, 0]]
  ];

  for (var [v,w] of valsExp) {
    testBinaryFunc(Int32x4(...v), Int32x4(...w), Int32x4.add, addi);
  }
}

function testInt32x4and() {
  function andi(a, b) {
    return (a & b) | 0;
  }

  var valsExp = [
    [[1, 2, 3, 4], [10, 20, 30, 40]],
    [[INT32_MAX, INT32_MIN, INT32_MAX, INT32_MIN], [INT32_MIN, INT32_MAX, INT32_MAX, INT32_MIN]]
  ];

  for (var [v,w] of valsExp) {
    testBinaryFunc(Int32x4(...v), Int32x4(...w), Int32x4.and, andi);
  }
}

function testInt32x4mul() {
  function muli(x, y) {
    return (x * y) | 0;
  }

  var valsExp = [
    [[1, 2, 3, 4], [10, 20, 30, 40]],
    [[INT32_MAX, INT32_MIN, INT32_MAX, INT32_MIN], [-1, -1, INT32_MIN, INT32_MIN]]
  ];

  for (var [v,w] of valsExp) {
    testBinaryFunc(Int32x4(...v), Int32x4(...w), Int32x4.mul, muli);
  }
}

function testInt32x4or() {
  function ori(a, b) {
    return (a | b) | 0;
  }

  var valsExp = [
    [[1, 2, 3, 4], [10, 20, 30, 40]],
    [[INT32_MAX, INT32_MIN, INT32_MAX, INT32_MIN], [INT32_MIN, INT32_MAX, INT32_MAX, INT32_MIN]]
  ];

  for (var [v,w] of valsExp) {
    testBinaryFunc(Int32x4(...v), Int32x4(...w), Int32x4.or, ori);
  }
}

function testInt32x4sub() {
  function subi(a, b) {
    return (a - b) | 0;
  }
  var valsExp = [
    [[10, 20, 30, 40], [1, 2, 3, 4]],
    [[INT32_MAX, INT32_MIN, INT32_MAX, INT32_MIN], [-1, 1, INT32_MAX, INT32_MIN]]
  ];

  for (var [v,w] of valsExp) {
    testBinaryFunc(Int32x4(...v), Int32x4(...w), Int32x4.sub, subi);
  }
}

function testInt32x4xor() {
  function xori(a, b) {
    return (a ^ b) | 0;
  }

  var valsExp = [
    [[1, 2, 3, 4], [10, 20, 30, 40]],
    [[INT32_MAX, INT32_MIN, INT32_MAX, INT32_MIN], [INT32_MIN, INT32_MAX, INT32_MAX, INT32_MIN]]
  ];

  for (var [v,w] of valsExp) {
    testBinaryFunc(Int32x4(...v), Int32x4(...w), Int32x4.xor, xori);
  }
}

var b8x16vals = [
  [[true, true, true, true, false, false, false, false, true, true, true, true, false, false, false, false],
   [false, true, false, true, false, true, false, true, true, true, true, true, false, false, false, false]]
];

function testBool8x16and() {
  function andb(a, b) {
    return a && b;
  }

  for (var [v,w] of b8x16vals) {
    testBinaryFunc(Bool8x16(...v), Bool8x16(...w), Bool8x16.and, andb);
  }
}

function testBool8x16or() {
  function orb(a, b) {
    return a || b;
  }

  for (var [v,w] of b8x16vals) {
    testBinaryFunc(Bool8x16(...v), Bool8x16(...w), Bool8x16.or, orb);
  }
}

function testBool8x16xor() {
  function xorb(a, b) {
    return a != b;
  }

  for (var [v,w] of b8x16vals) {
    testBinaryFunc(Bool8x16(...v), Bool8x16(...w), Bool8x16.xor, xorb);
  }
}

var b16x8vals = [
  [[true, true, true, true, false, false, false, false],
   [false, true, false, true, false, true, false, true]]
];

function testBool16x8and() {
  function andb(a, b) {
    return a && b;
  }

  for (var [v,w] of b16x8vals) {
    testBinaryFunc(Bool16x8(...v), Bool16x8(...w), Bool16x8.and, andb);
  }
}

function testBool16x8or() {
  function orb(a, b) {
    return a || b;
  }

  for (var [v,w] of b16x8vals) {
    testBinaryFunc(Bool16x8(...v), Bool16x8(...w), Bool16x8.or, orb);
  }
}

function testBool16x8xor() {
  function xorb(a, b) {
    return a != b;
  }

  for (var [v,w] of b16x8vals) {
    testBinaryFunc(Bool16x8(...v), Bool16x8(...w), Bool16x8.xor, xorb);
  }
}

var b32x4vals = [
  [[true, true, false, false], [false, true, false, true]]
];

function testBool32x4and() {
  function andb(a, b) {
    return a && b;
  }

  for (var [v,w] of b32x4vals) {
    testBinaryFunc(Bool32x4(...v), Bool32x4(...w), Bool32x4.and, andb);
  }
}

function testBool32x4or() {
  function orb(a, b) {
    return a || b;
  }

  for (var [v,w] of b32x4vals) {
    testBinaryFunc(Bool32x4(...v), Bool32x4(...w), Bool32x4.or, orb);
  }
}

function testBool32x4xor() {
  function xorb(a, b) {
    return a != b;
  }

  for (var [v,w] of b32x4vals) {
    testBinaryFunc(Bool32x4(...v), Bool32x4(...w), Bool32x4.xor, xorb);
  }
}

var b64x2vals = [
  [[false, false], [false, true], [true, false], [true, true]]
];

function testBool64x2and() {
  function andb(a, b) {
    return a && b;
  }

  for (var [v,w] of b64x2vals) {
    testBinaryFunc(Bool64x2(...v), Bool64x2(...w), Bool64x2.and, andb);
  }
}

function testBool64x2or() {
  function orb(a, b) {
    return a || b;
  }

  for (var [v,w] of b64x2vals) {
    testBinaryFunc(Bool64x2(...v), Bool64x2(...w), Bool64x2.or, orb);
  }
}

function testBool64x2xor() {
  function xorb(a, b) {
    return a != b;
  }

  for (var [v,w] of b64x2vals) {
    testBinaryFunc(Bool64x2(...v), Bool64x2(...w), Bool64x2.xor, xorb);
  }
}

function test() {
  testFloat32x4add();
  testFloat32x4and();
  testFloat32x4div();
  testFloat32x4mul();
  testFloat32x4or();
  testFloat32x4sub();
  testFloat32x4xor();

  testInt8x16add();
  testInt8x16and();
  testInt8x16mul();
  testInt8x16or();
  testInt8x16sub();
  testInt8x16xor();

  testInt16x8add();
  testInt16x8and();
  testInt16x8mul();
  testInt16x8or();
  testInt16x8sub();
  testInt16x8xor();

  testInt32x4add();
  testInt32x4and();
  testInt32x4mul();
  testInt32x4or();
  testInt32x4sub();
  testInt32x4xor();

  testBool8x16and();
  testBool8x16or();
  testBool8x16xor();

  testBool16x8and();
  testBool16x8or();
  testBool16x8xor();

  testBool32x4and();
  testBool32x4or();
  testBool32x4xor();

  testBool64x2and();
  testBool64x2or();
  testBool64x2xor();

  if (typeof reportCompare === "function") {
    reportCompare(true, true);
  }
}

test();
