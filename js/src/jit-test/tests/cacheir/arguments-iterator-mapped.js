// Test iteration with a mapped arguments object.

function simple() {
  function f() {
    var sum = 0;
    for (var v of arguments) {
      sum += v;
    }
    return sum;
  }

  for (var i = 0; i < 100; ++i) {
    assertEq(f(1, 2, 3), 6);
  }
}
simple();

function spreadCall() {
  function f() {
    var sum = 0;
    for (var v of arguments) {
      sum += v;
    }
    return sum;
  }

  function g() {
    return f(...arguments);
  }

  for (var i = 0; i < 100; ++i) {
    assertEq(g(1, 2, 3), 6);
  }
}
spreadCall();

function spreadArray() {
  function f() {
    var arr = [...arguments];
    var sum = 0;
    for (var v of arr) {
      sum += v;
    }
    return sum;
  }

  for (var i = 0; i < 100; ++i) {
    assertEq(f(1, 2, 3), 6);
  }
}
spreadArray();

function reifyIterator() {
  var reify = false;
  function f() {
    if (reify) {
      // Redefining any property attributes will reify the iterator property.
      Object.defineProperty(arguments, Symbol.iterator, {
        writable: false
      });
    }

    var sum = 0;
    for (var v of arguments) {
      sum += v;
    }
    return sum;
  }

  for (var i = 0; i <= 100; ++i) {
    reify = i >= 50;
    assertEq(f(1, 2, 3), 6);
  }
}
reifyIterator();

function overwriteIterator() {
  var callCount = 0;
  function Iterator() {
    callCount += 1;
    return Array.prototype[Symbol.iterator].call(this);
  }

  var overwrite = false;
  function f() {
    if (overwrite) {
      arguments[Symbol.iterator] = Iterator;
    }

    var sum = 0;
    for (var v of arguments) {
      sum += v;
    }
    return sum;
  }

  for (var i = 0; i <= 100; ++i) {
    overwrite = i > 50;
    assertEq(f(1, 2, 3), 6);
  }
  assertEq(callCount, 50);
}
overwriteIterator();

function deleteIterator() {
  var remove = false;
  function f() {
    // Deleting Symbol.iterator won't change the shape of the arguments object.
    // That's why we need to use a separate guard instruction to check if the
    // iterator property was modified.
    if (remove) {
      delete arguments[Symbol.iterator];
    }

    var sum = 0;
    for (var v of arguments) {
      sum += v;
    }
    return sum;
  }

  var error;
  try {
    for (var i = 0; i <= 100; ++i) {
      remove = i === 100;
      assertEq(f(1, 2, 3), 6);
    }
  } catch (e) {
    error = e;
  }
  assertEq(error instanceof TypeError, true);
}
deleteIterator();

function deleteIteratorInherit() {
  var callCount = 0;
  function Iterator() {
    callCount += 1;
    return Array.prototype[Symbol.iterator].call(this);
  }

  Object.prototype[Symbol.iterator] = Iterator;

  var remove = false;
  function f() {
    // Deleting Symbol.iterator won't change the shape of the arguments object.
    // That's why we need to use a separate guard instruction to check if the
    // iterator property was modified.
    if (remove) {
      delete arguments[Symbol.iterator];
    }

    var sum = 0;
    for (var v of arguments) {
      sum += v;
    }
    return sum;
  }

  for (var i = 0; i <= 100; ++i) {
    remove = i === 100;
    assertEq(f(1, 2, 3), 6);
  }
  assertEq(callCount, 1);

  delete Object.prototype[Symbol.iterator];
}
deleteIteratorInherit();

// Don't add tests below this point because |Object.prototype[Symbol.iterator]|
// was modified, which may lead to engine-wide deoptimisations.
