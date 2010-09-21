function f(h, i, Q) {
  var L = Q;
  var H = h;

  return h[i] * L ^ L * 0x1010100;
}
assertEq(f([6], 0, 12345), 1768429654);
