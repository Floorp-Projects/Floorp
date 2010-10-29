function deepForInLoop() {
  // NB: the number of props set in C is arefully tuned to match HOTLOOP = 2.
  function C(){this.p = 1, this.q = 2}
  C.prototype = {p:1, q:2, r:3, s:4, t:5};
  var o = new C;
  var j = 0;
  var a = [];
  for (var i in o)
    a[j++] = i;
  return a.join("");
}
assertEq(deepForInLoop(), "pqrst");
