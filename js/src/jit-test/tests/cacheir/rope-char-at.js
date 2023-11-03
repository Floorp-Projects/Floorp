function test(a, b, firstCharCode) {
  var s = newRope(a, b);
  for (var i = 0; i < s.length; i++) {
    assertEq(s.charCodeAt(i), firstCharCode + i);
    assertEq(s.charAt(i), String.fromCharCode(firstCharCode + i));
  }
  // charAt/charCodeAt support one-level deep ropes without linearizing.
  assertEq(isRope(s), true);
  assertEq(isRope(a), false);
  assertEq(isRope(b), false);
}
test("abcdefghijkl", "mnopqrstuvwxyz", 97);
test("a", "bcdefghijklmnopqrstuvwxyz", 97);
test("abcdefghijklmnopqrstuvwxy", "z", 97);
test("0123456789:;<=>?@ABCDEFGHIJ", "K", 48);
test("\u00fe\u00ff", "\u0100\u0101\u0102\u0103\u0104\u0105\u0106\u0107\u0108\u0109\u010A", 0xfe);
test("\u1000\u1001\u1002", "\u1003\u1004\u1005\u1006\u1007\u1008\u1009\u100A\u100B\u100C", 4096);

// charAt/charCodeAt stubs currently fail for nested ropes.
test("ab", newRope("cdefghijklmnop", "qrstuvwxyz{|}~"), 97);
