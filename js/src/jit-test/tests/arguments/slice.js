// Tests when |arguments| are used with Array.prototype.slice.

function testBasic() {
  // Return the number of arguments.
  function argslen() { return arguments.length; }

  // Return the first argument.
  function arg0() { return arguments[0]; }

  // Return the argument at the request index.
  function argIndex(i) { return arguments[i]; }

  // Call the above functions when no formals are present.
  function noFormalsLen() { return argslen(...Array.prototype.slice.call(arguments)); }
  function noFormals0() { return arg0(...Array.prototype.slice.call(arguments)); }
  function noFormalsIndex() { return argIndex(...Array.prototype.slice.call(arguments)); }

  // Call the above functions when some formals are present.
  function formalsLen(x, y, z) { return argslen(...Array.prototype.slice.call(arguments)); }
  function formals0(x, y, z) { return arg0(...Array.prototype.slice.call(arguments)); }
  function formalsIndex(x, y, z) { return argIndex(...Array.prototype.slice.call(arguments)); }

  // Call the above functions when a rest argument is present.
  function restLen(...rest) { return argslen(...Array.prototype.slice.call(arguments)); }
  function rest0(...rest) { return arg0(...Array.prototype.slice.call(arguments)); }
  function restIndex(...rest) { return argIndex(...Array.prototype.slice.call(arguments)); }

  // Call the above functions when default parameters are present.
  function defaultLen(x = 0) { return argslen(...Array.prototype.slice.call(arguments)); }
  function default0(x = 0) { return arg0(...Array.prototype.slice.call(arguments)); }
  function defaultIndex(x = 0) { return argIndex(...Array.prototype.slice.call(arguments)); }

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

// Same as testBasic, except that the first argument is removed.
function testRemoveFirst() {
  // Return the number of arguments.
  function argslen() { return arguments.length; }

  // Return the first argument.
  function arg0() { return arguments[0]; }

  // Return the argument at the request index.
  function argIndex(i) { return arguments[i]; }

  // Call the above functions when no formals are present.
  function noFormalsLen() { return argslen(...Array.prototype.slice.call(arguments, 1)); }
  function noFormals0() { return arg0(...Array.prototype.slice.call(arguments, 1)); }
  function noFormalsIndex() { return argIndex(...Array.prototype.slice.call(arguments, 1)); }

  // Call the above functions when some formals are present.
  function formalsLen(x, y, z) { return argslen(...Array.prototype.slice.call(arguments, 1)); }
  function formals0(x, y, z) { return arg0(...Array.prototype.slice.call(arguments, 1)); }
  function formalsIndex(x, y, z) { return argIndex(...Array.prototype.slice.call(arguments, 1)); }

  // Call the above functions when a rest argument is present.
  function restLen(...rest) { return argslen(...Array.prototype.slice.call(arguments, 1)); }
  function rest0(...rest) { return arg0(...Array.prototype.slice.call(arguments, 1)); }
  function restIndex(...rest) { return argIndex(...Array.prototype.slice.call(arguments, 1)); }

  // Call the above functions when default parameters are present.
  function defaultLen(x = 0) { return argslen(...Array.prototype.slice.call(arguments, 1)); }
  function default0(x = 0) { return arg0(...Array.prototype.slice.call(arguments, 1)); }
  function defaultIndex(x = 0) { return argIndex(...Array.prototype.slice.call(arguments, 1)); }

  for (let i = 0; i < 100; ++i) {
    assertEq(noFormalsLen(), 0);
    assertEq(noFormalsLen(1), 0);
    assertEq(noFormalsLen(1, 2, 3), 2);

    assertEq(formalsLen(), 0);
    assertEq(formalsLen(1), 0);
    assertEq(formalsLen(1, 2, 3), 2);

    assertEq(restLen(), 0);
    assertEq(restLen(1), 0);
    assertEq(restLen(1, 2, 3), 2);

    assertEq(defaultLen(), 0);
    assertEq(defaultLen(1), 0);
    assertEq(defaultLen(1, 2, 3), 2);

    assertEq(noFormals0(), undefined);
    assertEq(noFormals0(100), undefined);
    assertEq(noFormals0(100, 200, 300), 200);

    assertEq(formals0(), undefined);
    assertEq(formals0(100), undefined);
    assertEq(formals0(100, 200, 300), 200);

    assertEq(rest0(), undefined);
    assertEq(rest0(100), undefined);
    assertEq(rest0(100, 200, 300), 200);

    assertEq(default0(), undefined);
    assertEq(default0(100), undefined);
    assertEq(default0(100, 200, 300), 200);

    assertEq(noFormalsIndex(), undefined);
    assertEq(noFormalsIndex(0), undefined);
    assertEq(noFormalsIndex(0, 100), undefined);
    assertEq(noFormalsIndex(0, 0, 100), 0);
    assertEq(noFormalsIndex(0, 0, 100, 200, 300), 0);
    assertEq(noFormalsIndex(0, 1, 100), 100);
    assertEq(noFormalsIndex(0, 1, 100, 200, 300), 100);

    assertEq(formalsIndex(), undefined);
    assertEq(formalsIndex(0), undefined);
    assertEq(formalsIndex(0, 100), undefined);
    assertEq(formalsIndex(0, 0, 100), 0);
    assertEq(formalsIndex(0, 0, 100, 200, 300), 0);
    assertEq(formalsIndex(0, 1, 100), 100);
    assertEq(formalsIndex(0, 1, 100, 200, 300), 100);

    assertEq(restIndex(), undefined);
    assertEq(restIndex(0), undefined);
    assertEq(restIndex(0, 0), 0);
    assertEq(restIndex(0, 0, 100), 0);
    assertEq(restIndex(0, 0, 100, 200, 300), 0);
    assertEq(restIndex(0, 1, 100), 100);
    assertEq(restIndex(0, 1, 100, 200, 300), 100);

    assertEq(defaultIndex(), undefined);
    assertEq(defaultIndex(0), undefined);
    assertEq(defaultIndex(0, 0), 0);
    assertEq(defaultIndex(0, 0, 100), 0);
    assertEq(defaultIndex(0, 0, 100, 200, 300), 0);
    assertEq(defaultIndex(0, 1, 100), 100);
    assertEq(defaultIndex(0, 1, 100, 200, 300), 100);
  }
}
testRemoveFirst();

// Same as testBasic, except that the last argument is removed.
function testRemoveLast() {
  // Return the number of arguments.
  function argslen() { return arguments.length; }

  // Return the first argument.
  function arg0() { return arguments[0]; }

  // Return the argument at the request index.
  function argIndex(i) { return arguments[i]; }

  // Call the above functions when no formals are present.
  function noFormalsLen() { return argslen(...Array.prototype.slice.call(arguments, 0, -1)); }
  function noFormals0() { return arg0(...Array.prototype.slice.call(arguments, 0, -1)); }
  function noFormalsIndex() { return argIndex(...Array.prototype.slice.call(arguments, 0, -1)); }

  // Call the above functions when some formals are present.
  function formalsLen(x, y, z) { return argslen(...Array.prototype.slice.call(arguments, 0, -1)); }
  function formals0(x, y, z) { return arg0(...Array.prototype.slice.call(arguments, 0, -1)); }
  function formalsIndex(x, y, z) { return argIndex(...Array.prototype.slice.call(arguments, 0, -1)); }

  // Call the above functions when a rest argument is present.
  function restLen(...rest) { return argslen(...Array.prototype.slice.call(arguments, 0, -1)); }
  function rest0(...rest) { return arg0(...Array.prototype.slice.call(arguments, 0, -1)); }
  function restIndex(...rest) { return argIndex(...Array.prototype.slice.call(arguments, 0, -1)); }

  // Call the above functions when default parameters are present.
  function defaultLen(x = 0) { return argslen(...Array.prototype.slice.call(arguments, 0, -1)); }
  function default0(x = 0) { return arg0(...Array.prototype.slice.call(arguments, 0, -1)); }
  function defaultIndex(x = 0) { return argIndex(...Array.prototype.slice.call(arguments, 0, -1)); }

  for (let i = 0; i < 100; ++i) {
    assertEq(noFormalsLen(), 0);
    assertEq(noFormalsLen(1), 0);
    assertEq(noFormalsLen(1, 2, 3), 2);

    assertEq(formalsLen(), 0);
    assertEq(formalsLen(1), 0);
    assertEq(formalsLen(1, 2, 3), 2);

    assertEq(restLen(), 0);
    assertEq(restLen(1), 0);
    assertEq(restLen(1, 2, 3), 2);

    assertEq(defaultLen(), 0);
    assertEq(defaultLen(1), 0);
    assertEq(defaultLen(1, 2, 3), 2);

    assertEq(noFormals0(), undefined);
    assertEq(noFormals0(100), undefined);
    assertEq(noFormals0(100, 200, 300), 100);

    assertEq(formals0(), undefined);
    assertEq(formals0(100), undefined);
    assertEq(formals0(100, 200, 300), 100);

    assertEq(rest0(), undefined);
    assertEq(rest0(100), undefined);
    assertEq(rest0(100, 200, 300), 100);

    assertEq(default0(), undefined);
    assertEq(default0(100), undefined);
    assertEq(default0(100, 200, 300), 100);

    assertEq(noFormalsIndex(), undefined);
    assertEq(noFormalsIndex(0), undefined);
    assertEq(noFormalsIndex(0, 100), 0);
    assertEq(noFormalsIndex(0, 100, 200, 300), 0);
    assertEq(noFormalsIndex(1, 100), undefined);
    assertEq(noFormalsIndex(1, 100, 200, 300), 100);

    assertEq(formalsIndex(), undefined);
    assertEq(formalsIndex(0), undefined);
    assertEq(formalsIndex(0, 100), 0);
    assertEq(formalsIndex(0, 100, 200, 300), 0);
    assertEq(formalsIndex(1, 100), undefined);
    assertEq(formalsIndex(1, 100, 200, 300), 100);

    assertEq(restIndex(), undefined);
    assertEq(restIndex(0), undefined);
    assertEq(restIndex(0, 100), 0);
    assertEq(restIndex(0, 100, 200, 300), 0);
    assertEq(restIndex(1, 100), undefined);
    assertEq(restIndex(1, 100, 200, 300), 100);

    assertEq(defaultIndex(), undefined);
    assertEq(defaultIndex(0), undefined);
    assertEq(defaultIndex(0, 100), 0);
    assertEq(defaultIndex(0, 100, 200, 300), 0);
    assertEq(defaultIndex(1, 100), undefined);
    assertEq(defaultIndex(1, 100, 200, 300), 100);
  }
}
testRemoveLast();

function testOverriddenLength() {
  function g(x, y = 0) {
    return x + y;
  }

  function f(i) {
    if (i === 100) {
      arguments.length = 1;
    }
    var args = Array.prototype.slice.call(arguments);
    return g.apply(null, args);
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
    var args = Array.prototype.slice.call(arguments);
    return g(args[0], args[1]);
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
    var args = Array.prototype.slice.call(arguments);
    return g.apply(null, args);
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
    var args = Array.prototype.slice.call(arguments);
    return g(args[0], args[1]);
  }

  for (let i = 0; i <= 150; ++i) {
    assertEq(f(i, i), i !== 100 ? i * 2 : i);
  }
}
testForwardedArg();
