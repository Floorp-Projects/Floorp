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
test("abcdef", "ghijk", 97);
test("", "abcdefg", 97);
test("abcdef", "", 97);
test("0123456", "7", 48);
test("\u00fe\u00ff", "\u0100\u0101", 0xfe);
test("\u1000\u1001\u1002", "\u1003\u1004", 4096);

// charAt/charCodeAt stubs currently fail for nested ropes.
test("abcdef", newRope("ghij", "klmn"), 97);
