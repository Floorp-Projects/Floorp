var test;
{
  let a = 5;
    with ({a: 2}) {
      test = (function () { return a; })();
    }
}
assertEq(test, 2);
