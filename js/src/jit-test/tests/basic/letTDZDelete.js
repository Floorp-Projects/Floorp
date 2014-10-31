function assertThrowsReferenceError(f) {
  var e = null;
  try {
    f();
  } catch (ex) {
    e = ex;
  }
  assertEq(e instanceof ReferenceError, true);
}

assertThrowsReferenceError(function () { delete x; let x; });
assertThrowsReferenceError(function () { delete x; const x = undefined; });
