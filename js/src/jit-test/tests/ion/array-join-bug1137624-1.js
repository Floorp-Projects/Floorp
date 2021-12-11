
try {
  var x = ["a", {toString() { s = x.join("-"); }}];
  var a = x.join("a");
  var b = x.join("b");
  assertEq(a, b);
} catch (e) {
  // Using assertion does not work.
  quit(0);
}

quit(3);
