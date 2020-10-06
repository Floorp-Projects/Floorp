// |reftest| skip-if(!String.prototype.item)

function basic() {
  assertEq("a".item(0), "a");
  assertEq("a".item(-1), "a");

  assertEq("".item(0), undefined);
  assertEq("".item(-1), undefined);
  assertEq("".item(1), undefined);

  assertEq("ab".item(0), "a");
  assertEq("ab".item(1), "b");
  assertEq("ab".item(-2), "a");
  assertEq("ab".item(-1), "b");

  assertEq("ab".item(2), undefined);
  assertEq("ab".item(-3), undefined);
  assertEq("ab".item(-4), undefined);
  assertEq("ab".item(Infinity), undefined);
  assertEq("ab".item(-Infinity), undefined);
  assertEq("ab".item(NaN), "a"); // ToInteger(NaN) = 0

  assertEq("\u{1F921}".item(0), "\u{D83E}");
  assertEq("\u{1F921}".item(1), "\u{DD21}");
}

basic();

if (typeof reportCompare === "function")
    reportCompare(true, true);
