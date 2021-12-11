function foo() {
  var x = 0;
  var y = 0;
  while (x++ < 100)
    y++;
  assertEq(y, 100);
}
foo();
