function test() {
  var nullUndefLhs = [
    [null, "foo", "nullfoo"],
    [null, "bar", "nullbar"],
    [null, "123", "null123"],
    [null, "", "null"],
    [undefined, "abc", "undefinedabc"],
    [undefined, "1", "undefined1"],
    [undefined, "", "undefined"],
    ["abc", "def", "abcdef"],
  ];
  for (var [lhs, rhs, expected] of nullUndefLhs) {
    assertEq(lhs + rhs, expected);
  }
  var nullUndefRhs = [
    ["foo", undefined, "fooundefined"],
    ["bar", undefined, "barundefined"],
    ["123", undefined, "123undefined"],
    ["", undefined, "undefined"],
    ["abc", null, "abcnull"],
    ["1", null, "1null"],
    ["", null, "null"],
    ["abc", "def", "abcdef"],
  ];
  for (var [lhs, rhs, expected] of nullUndefRhs) {
    assertEq(lhs + rhs, expected);
  }
}
test();
test();
test();
