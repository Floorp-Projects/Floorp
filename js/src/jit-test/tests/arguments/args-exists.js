// Test |in| operator on arguments objects.

function testBasic() {
  function arg0() { return 0 in arguments; }
  function arg1() { return 1 in arguments; }
  function arg100() { return 100 in arguments; }
  function argn1() { return -1 in arguments; }
  function argv(i) { return i in arguments; }

  for (let i = 0; i < 100; ++i) {
    assertEq(arg0(), false);
    assertEq(arg0(1), true);
    assertEq(arg0(1, 2), true);
    assertEq(arg0(1, 2, 3), true);
    assertEq(arg0(1, 2, 3, 4), true);
    assertEq(arg0(1, 2, 3, 4, 5), true);

    assertEq(arg1(), false);
    assertEq(arg1(1), false);
    assertEq(arg1(1, 2), true);
    assertEq(arg1(1, 2, 3), true);
    assertEq(arg1(1, 2, 3, 4), true);
    assertEq(arg1(1, 2, 3, 4, 5), true);

    assertEq(arg100(), false);
    assertEq(arg100(1), false);
    assertEq(arg100(1, 2), false);
    assertEq(arg100(1, 2, 3), false);
    assertEq(arg100(1, 2, 3, 4), false);
    assertEq(arg100(1, 2, 3, 4, 5), false);

    assertEq(argn1(), false);
    assertEq(argn1(1), false);
    assertEq(argn1(1, 2), false);
    assertEq(argn1(1, 2, 3), false);
    assertEq(argn1(1, 2, 3, 4), false);
    assertEq(argn1(1, 2, 3, 4, 5), false);

    assertEq(argv(i & 3), (i&3) <= 0);
    assertEq(argv(i & 3, 1), (i&3) <= 1);
    assertEq(argv(i & 3, 1, 2), (i&3) <= 2);
    assertEq(argv(i & 3, 1, 2, 3), true);
    assertEq(argv(i & 3, 1, 2, 3, 4), true);
    assertEq(argv(i & 3, 1, 2, 3, 4, 5), true);
  }
}
testBasic();

// Modifying |arguments.length| doesn't change the result.
function testOverriddenLength() {
  function f(i) {
    if (i === 100) {
      arguments.length = 100;
    }
    return 1 in arguments;
  }

  for (let i = 0; i <= 150; ++i) {
    assertEq(f(i), false);
  }
}
testOverriddenLength();

// Overridden elements are correctly detected.
function testOverriddenElement() {
  function f(i) {
    if (i === 100) {
      arguments[1] = 0;
    }
    return 1 in arguments;
  }

  for (let i = 0; i <= 150; ++i) {
    assertEq(f(i), i === 100);
  }
}
testOverriddenElement();

// Deleted elements are correctly detected.
function testDeletedElement() {
  function f(i) {
    if (i === 100) {
      delete arguments[0];
    }
    return 0 in arguments;
  }

  for (let i = 0; i <= 150; ++i) {
    assertEq(f(i), i !== 100);
  }
}
testDeletedElement();

// Contrary to most other arguments object optimisations, forwarded arguments
// don't inhibit optimising the |in| operator.
function testForwardedArg() {
  function f(i) {
    function closedOver() {
      if (i === 100) i = 0;
    }
    closedOver();
    return 0 in arguments;
  }

  for (let i = 0; i <= 150; ++i) {
    assertEq(f(i), true);
  }
}
testForwardedArg();
