function g() {
  var UPPER_MASK = 2147483648
  var mt = new Array
  function f1(n1) {
    return n1 < 0 ? (n1 ^ UPPER_MASK) + UPPER_MASK: n1
  }
  function f2(n1, n2) {
    return f1(n1 + n2 & 4294967295)
  }
  function f3(n1, n2) {
    var sum
    for (var i = 0; i < 32; ++i) {
      sum = f2(sum, f1(n2 << i))
    }
    return sum
  }
  this.init_genrand = function(s) {
    mt[0] = f1(s & 96295)
    for (mti = 1; mti < 6; mti++) {
      mt[mti] = f2(f3(3, f1(mt[mti - 1] ^ mt[1] > 0)), mti)
    }
  }
} (function() {
  var fuzzMT = new g;
  fuzzMT.init_genrand(54)
} ())

/* Don't assert. */

