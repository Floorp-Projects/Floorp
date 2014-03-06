load(libdir + "parallelarray-helpers.js");

function testClosureCreationAndInvocation() {
  var a = range(0, 64);
  function makeaddv(v) {
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
              }
            });
  }
  var m;
  for (var i in MODES) m = a.mapPar(makeaddv, MODES[i]);
  assertEq(m[21](1), 20); // v == 21; x == 1 ==> inner function returns b == 20

  var n = a.mapPar(function (v) { return function (x) { return v; }});
  assertEq(n[21](1), 21); // v == 21
}

if (getBuildConfiguration().parallelJS)
  testClosureCreationAndInvocation();
