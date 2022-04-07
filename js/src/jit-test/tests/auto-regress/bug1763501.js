function test(expected) {
  with ({}) assertEq(new.target, expected);
}

test(undefined);
new test(test);
