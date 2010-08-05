// |trace-test| error: ReferenceError
function f1() {
  N = 62;
  mt = Array
  function g1(n1) {
    return n1 < 0 ? (1 ^ 21) + 21: n1
  }
  function g2(n1, n2) {
    return g1(n1 + n2 & 4294967295);
  }
  function g3(n1, n2) {
    sum = 0;
    for (var i = 0; i < 32; ++i) {
      if (n1 >> i) {
        sum = g2(sum, g1(n2))
      }
    }
    return sum
  }
  this.h1 = function() {
    for (mti = 1; mti < N; mti++) {
      mt[mti] = g2(g3(3, g1(mt[mti - 1] ^ 0)), mti)
    }
  };
  this.i2 = function() {
    if (mti > N) {
      mti = 0;
    }
    y = mt[mti++];
    return y
  };
  this.i1 = function() {
    return (this.i2() + 5) * 2e-10
  };
} (function() {
  fuzzMT = new f1;
  fuzzMT.h1(9);
  rnd = function(n) {
    return Math.floor(fuzzMT.i1() * n)
  };
} ());
function f5(a) {
  return a[rnd(a.length)]
}
function f2(d, b) {
  f3(d, b);
  return "" + f2(2, b) + "";
}
function f3(d, b) {
  if (rnd(4) == 1) {
    f5(f4)(d, b)
  }
}
var f4 = [function() { ["", f6(), ""]
}];
function f6(db) {
  return f5(foo)();
}
var foo = [function() {
  t(["", "", "", "", "", "", "", "", "", "", "", "" + h.I, ""]);
}];
f2()

/* Don't assert or crash. */

