// Tests when |arguments| are used in optimized spread calls.

function testBasic() {
  // Return the number of arguments.
  function argslen() { return arguments.length; }

  // Return the first argument.
  function arg0() { return arguments[0]; }

  // Return the argument at the request index.
  function argIndex(i) { return arguments[i]; }

  // Call the above functions when no formals are present.
  function noFormalsLen() { return argslen(...arguments); }
  function noFormals0() { return arg0(...arguments); }
  function noFormalsIndex() { return argIndex(...arguments); }

  // Call the above functions when some formals are present.
  function formalsLen(x, y, z) { return argslen(...arguments); }
  function formals0(x, y, z) { return arg0(...arguments); }
  function formalsIndex(x, y, z) { return argIndex(...arguments); }

  // Call the above functions when a rest argument is present.
  function restLen(...rest) { return argslen(...arguments); }
  function rest0(...rest) { return arg0(...arguments); }
  function restIndex(...rest) { return argIndex(...arguments); }

  // Call the above functions when default parameters are present.
  function defaultLen(x = 0) { return argslen(...arguments); }
  function default0(x = 0) { return arg0(...arguments); }
  function defaultIndex(x = 0) { return argIndex(...arguments); }

  for (let i = 0; i < 100; ++i) {
    assertEq(noFormalsLen(), 0);
    assertEq(noFormalsLen(1), 1);
    assertEq(noFormalsLen(1, 2, 3), 3);

    assertEq(formalsLen(), 0);
    assertEq(formalsLen(1), 1);
    assertEq(formalsLen(1, 2, 3), 3);

    assertEq(restLen(), 0);
    assertEq(restLen(1), 1);
    assertEq(restLen(1, 2, 3), 3);

    assertEq(defaultLen(), 0);
    assertEq(defaultLen(1), 1);
    assertEq(defaultLen(1, 2, 3), 3);

    assertEq(noFormals0(), undefined);
    assertEq(noFormals0(100), 100);
    assertEq(noFormals0(100, 200, 300), 100);

    assertEq(formals0(), undefined);
    assertEq(formals0(100), 100);
    assertEq(formals0(100, 200, 300), 100);

    assertEq(rest0(), undefined);
    assertEq(rest0(100), 100);
    assertEq(rest0(100, 200, 300), 100);

    assertEq(default0(), undefined);
    assertEq(default0(100), 100);
    assertEq(default0(100, 200, 300), 100);

    assertEq(noFormalsIndex(), undefined);
    assertEq(noFormalsIndex(0), 0);
    assertEq(noFormalsIndex(0, 100), 0);
    assertEq(noFormalsIndex(0, 100, 200, 300), 0);
    assertEq(noFormalsIndex(1, 100), 100);
    assertEq(noFormalsIndex(1, 100, 200, 300), 100);

    assertEq(formalsIndex(), undefined);
    assertEq(formalsIndex(0), 0);
    assertEq(formalsIndex(0, 100), 0);
    assertEq(formalsIndex(0, 100, 200, 300), 0);
    assertEq(formalsIndex(1, 100), 100);
    assertEq(formalsIndex(1, 100, 200, 300), 100);

    assertEq(restIndex(), undefined);
    assertEq(restIndex(0), 0);
    assertEq(restIndex(0, 100), 0);
    assertEq(restIndex(0, 100, 200, 300), 0);
    assertEq(restIndex(1, 100), 100);
    assertEq(restIndex(1, 100, 200, 300), 100);

    assertEq(defaultIndex(), undefined);
    assertEq(defaultIndex(0), 0);
    assertEq(defaultIndex(0, 100), 0);
    assertEq(defaultIndex(0, 100, 200, 300), 0);
    assertEq(defaultIndex(1, 100), 100);
    assertEq(defaultIndex(1, 100, 200, 300), 100);
  }
}
testBasic();

function testOverriddenIterator() {
  function g(x) {
    return x;
  }

  function f(i) {
    if (i === 100) {
      arguments[Symbol.iterator] = function() {
        return ["bad"].values();
      };
    }
    return g(...arguments);
  }

  for (let i = 0; i <= 150; ++i) {
    assertEq(f(i), i !== 100 ? i : "bad");
  }
}
testOverriddenIterator();

function testOverriddenLength() {
  function g(x, y = 0) {
    return x + y;
  }

  function f(i) {
    if (i === 100) {
      arguments.length = 1;
    }
    return g(...arguments);
  }

  for (let i = 0; i <= 150; ++i) {
    assertEq(f(i, i), i !== 100 ? i * 2 : i);
  }
}
testOverriddenLength();

function testOverriddenElement() {
  function g(x, y) {
    return x + y;
  }

  function f(i) {
    if (i === 100) {
      arguments[1] = 0;
    }
    return g(...arguments);
  }

  for (let i = 0; i <= 150; ++i) {
    assertEq(f(i, i), i !== 100 ? i * 2 : i);
  }
}
testOverriddenElement();

function testDeletedElement() {
  function g(x, y = 0) {
    return x + y;
  }

  function f(i) {
    if (i === 100) {
      delete arguments[1];
    }
    return g(...arguments);
  }

  for (let i = 0; i <= 150; ++i) {
    assertEq(f(i, i), i !== 100 ? i * 2 : i);
  }
}
testDeletedElement();

function testForwardedArg() {
  function g(x, y) {
    return x + y;
  }

  function f(i) {
    function closedOver() {
      if (i === 100) i = 0;
    }
    closedOver();
    return g(...arguments);
  }

  for (let i = 0; i <= 150; ++i) {
    assertEq(f(i, i), i !== 100 ? i * 2 : i);
  }
}
testForwardedArg();

function testModifiedArrayIteratorPrototype() {
  function g(x, y) {
    return x + y;
  }

  const ArrayIteratorPrototype = Reflect.getPrototypeOf([][Symbol.iterator]());
  const ArrayIteratorPrototypeNext = ArrayIteratorPrototype.next;
  function newNext() {
    var result = ArrayIteratorPrototypeNext.call(this);
    if (!result.done) {
      result.value *= 2;
    }
    return result;
  }

  function f(i) {
    if (i === 100) {
      ArrayIteratorPrototype.next = newNext;
    }
    return g(...arguments);
  }

  for (let i = 0; i <= 150; ++i) {
    assertEq(f(i, i), i < 100 ? i * 2 : i * 4);
  }
}
testModifiedArrayIteratorPrototype();
