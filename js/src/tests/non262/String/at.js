function basic() {
  assertEq("a".at(0), "a");
  assertEq("a".at(-1), "a");

  assertEq("".at(0), undefined);
  assertEq("".at(-1), undefined);
  assertEq("".at(1), undefined);

  assertEq("ab".at(0), "a");
  assertEq("ab".at(1), "b");
  assertEq("ab".at(-2), "a");
  assertEq("ab".at(-1), "b");

  assertEq("ab".at(2), undefined);
  assertEq("ab".at(-3), undefined);
  assertEq("ab".at(-4), undefined);
  assertEq("ab".at(Infinity), undefined);
  assertEq("ab".at(-Infinity), undefined);
  assertEq("ab".at(NaN), "a"); // ToInteger(NaN) = 0

  assertEq("\u{1f921}".at(0), "\u{d83e}");
  assertEq("\u{1f921}".at(1), "\u{dd21}");
}

function other() {
  var n = 146;
  assertEq(String.prototype.at.call(n, 0), "1");
  var obj = {};
  assertEq(String.prototype.at.call(obj, -1), "]");
  var b = true;
  assertEq(String.prototype.at.call(b, 0), "t");
}

basic();
other();

if (typeof reportCompare === "function")
    reportCompare(true, true);
