load(libdir + "parallelarray-helpers.js");

function testClosureCreationAndInvocation() {
  var a = range(0, 64);
  var p = new ParallelArray(a);
  function makeaddv(v) {
    var u = v - 1;
    var t = v - 2;
    var s = v - 3;
    var r = v - 4;
    var q = v - 5;
    var p = v - 6;
    var o = v - 7;
    var n = v - 8;
    var m = v - 9;
    var l = v - 10;
    var k = v - 11;
    var j = v - 12;
    var i = v - 13;
    var h = v - 14;
    var g = v - 15;
    var f = v - 16;
    var e = v - 17;
    var d = v - 18;
    var c = v - 19;
    var b = v - 20;
    var a = v - 21;
    return function (x) {
      switch (x) {
      case 0: return a; case 1: return b;
      case 2: return c; case 3: return d;
      case 4: return e; case 5: return f;
      case 6: return g; case 7: return h;
      case 8: return i; case 9: return j;
      case 10: return k; case 11: return l;
      case 12: return m; case 13: return n;
      case 14: return o; case 15: return p;
      case 16: return q; case 17: return r;
      case 18: return s; case 19: return t;
      case 20: return u;
      }
    };
  };
  var m = p.map(makeaddv);
  assertEq(m.get(21)(1), 1); // v == 21; x == 1 ==> inner function returns b == 1
}

testClosureCreationAndInvocation();
