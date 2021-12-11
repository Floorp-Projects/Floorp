
function f56(x) {
  var a = x >>> 0; // Range = [0 .. UINT32_MAX] (bits = 32)
  var b = 0x800000; // == 2^23 (bits = 24)
  if (a > 0) {
    // Beta node: Range [1 .. UINT32_MAX] (bits = 32)
    var c = a * b; // Range = [0 .. +inf] (bits = a.bits + b.bits - 1 = 55)
    var d = c + 1; // Range = [0 .. +inf] (bits = c.bits + 1 = 56)
    return (d | 0) & 1;
  } else {
    return 1;
  }
}

function f55(x) {
  var a = x >>> 0; // Range = [0 .. UINT32_MAX] (bits = 32)
  var b = 0x400000; // == 2^22 (bits = 23)
  if (a > 0) {
    // Beta node: Range [1 .. UINT32_MAX] (bits = 32)
    var c = a * b; // Range = [0 .. +inf] (bits = a.bits + b.bits - 1 = 54)
    var d = c + 1; // Range = [0 .. +inf] (bits = c.bits + 1 = 55)
    return (d | 0) & 1;
  } else {
    return 1;
  }
}

// Still returns 1, because the top-level bit is not represented.
function f54(x) {
  var a = x >>> 0; // Range = [0 .. UINT32_MAX] (bits = 32)
  var b = 0x200000; // == 2^21 (bits = 22)
  if (a > 0) {
    // Beta node: Range [1 .. UINT32_MAX] (bits = 32)
    var c = a * b; // Range = [0 .. +inf] (bits = a.bits + b.bits - 1 = 53)
    var d = c + 1; // Range = [1 .. +inf] (bits = c.bits + 1 = 54)
    return (d | 0) & 1;
  } else {
    return 1;
  }
}

// Can safely truncate after these operations. (the mantissa has 53 bits)
function f53(x) {
  var a = x >>> 0; // Range = [0 .. UINT32_MAX] (bits = 32)
  var b = 0x100000; // == 2^20 (bits = 21)
  if (a > 0) {
    // Beta node: Range [1 .. UINT32_MAX] (bits = 32)
    var c = a * b; // Range = [0 .. +inf] (bits = a.bits + b.bits - 1 = 52)
    var d = c + 1; // Range = [1 .. +inf] (bits = c.bits + 1 = 53)
    return (d | 0) & 1;
  } else {
    return 1;
  }
}

function f52(x) {
  var a = x >>> 0; // Range = [0 .. UINT32_MAX] (bits = 32)
  var b = 0x80000; // == 2^19 (bits = 20)
  if (a > 0) {
    // Beta node: Range [1 .. UINT32_MAX] (bits = 32)
    var c = a * b; // Range = [0 .. +inf] (bits = a.bits + b.bits - 1 = 51)
    var d = c + 1; // Range = [1 .. +inf] (bits = c.bits + 1 = 52)
    return (d | 0) & 1;
  } else {
    return 1;
  }
}

function f51(x) {
  var a = x >>> 0; // Range = [0 .. UINT32_MAX] (bits = 32)
  var b = 0x40000; // == 2^18 (bits = 19)
  if (a > 0) {
    // Beta node: Range [1 .. UINT32_MAX] (bits = 32)
    var c = a * b; // Range = [0 .. +inf] (bits = a.bits + b.bits - 1 = 50)
    var d = c + 1; // Range = [1 .. +inf] (bits = c.bits + 1 = 51)
    return (d | 0) & 1;
  } else {
    return 1;
  }
}

var e = Math.pow(2, 32);
for (var i = 1; i < e; i = i * 1.5) {
    var x = i >>> 0;
    assertEq(f56(x) , (x >= Math.pow(2, 30)) ? 0 : 1);
    assertEq(f55(x), (x >= Math.pow(2, 31)) ? 0 : 1);
    assertEq(f54(x), 1);
    assertEq(f53(x), 1);
    assertEq(f52(x), 1);
    assertEq(f51(x), 1);
}
