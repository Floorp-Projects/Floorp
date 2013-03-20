load(libdir + "parallelarray-helpers.js");

function testClosureCreationAndInvocation() {
  var a = range(0, 64);
  var p = new ParallelArray(a);
  function makeaddv(v) {
    var u = 1;
    var t = 2;
    var s = 3;
    var r = 4;
    var q = 5;
    var p = 6;
    var o = 7;
    var n = 8;
    var m = 9;
    var l = 10;
    var k = 11;
    var j = 12;
    var i = 13;
    var h = 14;
    var g = 15;
    var f = 16;
    var e = 17;
    var d = 18;
    var c = 19;
    var b = 20;
    var a = 21;
    return ((v % 2 == 0)
            ? function (x) { return a; }
            : function (x) {
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
            });
  }
  var m = p.map(makeaddv, {mode: "par", expect: "success"});
  assertEq(m.get(21)(1), 20); // v == 21; x == 1 ==> inner function returns b == 20

  var n = p.map(function (v) { return function (x) { return v; }});
  assertEq(n.get(21)(1), 21); // v == 21
}

if (getBuildConfiguration().parallelJS)
  testClosureCreationAndInvocation();
