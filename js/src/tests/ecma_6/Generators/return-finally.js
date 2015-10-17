var BUGNUMBER = 1202134;
var summary = "Return value should not be overwritten by finally block with normal execution.";

print(BUGNUMBER + ": " + summary);

// ==== single ====

var f, g, v;
f = function*() {
  // F.[[type]] is normal
  // B.[[type]] is return
  try {
    return 42;
  } finally {
  }
};
g = f();
v = g.next();
assertEq(v.value, 42);
assertEq(v.done, true);

f = function*() {
  // F.[[type]] is return
  try {
    return 42;
  } finally {
    return 43;
  }
};
g = f();
v = g.next();
assertEq(v.value, 43);
assertEq(v.done, true);

f = function*() {
  // F.[[type]] is throw
  try {
    return 42;
  } finally {
    throw 43;
  }
};
var caught = false;
g = f();
try {
  v = g.next();
} catch (e) {
  assertEq(e, 43);
  caught = true;
}
assertEq(caught, true);

f = function*() {
  // F.[[type]] is break
  do try {
    return 42;
  } finally {
    break;
  } while (false);
  return 43;
};
g = f();
v = g.next();
assertEq(v.value, 43);
assertEq(v.done, true);

f = function*() {
  // F.[[type]] is break
  L: try {
    return 42;
  } finally {
    break L;
  }
  return 43;
};
g = f();
v = g.next();
assertEq(v.value, 43);
assertEq(v.done, true);

f = function*() {
  // F.[[type]] is continue
  do try {
    return 42;
  } finally {
    continue;
  } while (false);
  return 43;
};
g = f();
v = g.next();
assertEq(v.value, 43);
assertEq(v.done, true);

// ==== nested ====

f = function*() {
  // F.[[type]] is normal
  // B.[[type]] is return
  try {
    return 42;
  } finally {
    // F.[[type]] is break
    do try {
      return 43;
    } finally {
      break;
    } while (0);
  }
};
g = f();
v = g.next();
assertEq(v.value, 42);
assertEq(v.done, true);

f = function*() {
  // F.[[type]] is normal
  // B.[[type]] is return
  try {
    return 42;
  } finally {
    // F.[[type]] is break
    L: try {
      return 43;
    } finally {
      break L;
    }
  }
}
g = f();
v = g.next();
assertEq(v.value, 42);
assertEq(v.done, true);

f = function*() {
  // F.[[type]] is normal
  // B.[[type]] is return
  try {
    return 42;
  } finally {
    // F.[[type]] is continue
    do try {
      return 43;
    } finally {
      continue;
    } while (0);
  }
};
g = f();
v = g.next();
assertEq(v.value, 42);
assertEq(v.done, true);

f = function*() {
  // F.[[type]] is normal
  // B.[[type]] is return
  try {
    return 42;
  } finally {
    // F.[[type]] is normal
    // B.[[type]] is normal
    try {
      // F.[[type]] is throw
      try {
        return 43;
      } finally {
        throw 9;
      }
    } catch (e) {
    }
  }
};
g = f();
v = g.next();
assertEq(v.value, 42);
assertEq(v.done, true);

f = function*() {
  // F.[[type]] is return
  try {
    return 41;
  } finally {
    // F.[[type]] is normal
    // B.[[type]] is return
    try {
      return 42;
    } finally {
      // F.[[type]] is break
      do try {
        return 43;
      } finally {
        break;
      } while (0);
    }
  }
};
g = f();
v = g.next();
assertEq(v.value, 42);
assertEq(v.done, true);

// ==== with yield ====

f = function*() {
  // F.[[type]] is normal
  // B.[[type]] is return
  try {
    return 42;
  } finally {
    yield 43;
  }
};
g = f();
v = g.next();
assertEq(v.value, 43);
assertEq(v.done, false);
v = g.next();
assertEq(v.value, 42);
assertEq(v.done, true);

// ==== throw() ====

f = function*() {
  // F.[[type]] is throw
  try {
    return 42;
  } finally {
    yield 43;
  }
};
caught = false;
g = f();
v = g.next();
assertEq(v.value, 43);
assertEq(v.done, false);
try {
  v = g.throw(44);
} catch (e) {
  assertEq(e, 44);
  caught = true;
}
assertEq(caught, true);

f = function*() {
  // F.[[type]] is normal
  try {
    return 42;
  } finally {
    // F.[[type]] is normal
    // B.[[type]] is throw
    try {
      yield 43;
    } catch (e) {
    }
  }
};
caught = false;
g = f();
v = g.next();
assertEq(v.value, 43);
assertEq(v.done, false);
v = g.throw(44);
assertEq(v.value, 42);
assertEq(v.done, true);

// ==== return() ====

f = function*() {
  // F.[[type]] is return
  try {
    return 42;
  } finally {
    yield 43;
  }
};
caught = false;
g = f();
v = g.next();
assertEq(v.value, 43);
assertEq(v.done, false);
v = g.return(44);
assertEq(v.value, 44);
assertEq(v.done, true);

f = function*() {
  // F.[[type]] is normal
  // B.[[type]] is return
  try {
    yield 42;
  } finally {
    // F.[[type]] is continue
    do try {
      return 43;
    } finally {
      continue;
    } while (0);
  }
};
caught = false;
g = f();
v = g.next();
assertEq(v.value, 42);
assertEq(v.done, false);
v = g.return(44);
assertEq(v.value, 44);
assertEq(v.done, true);

if (typeof reportCompare === "function")
    reportCompare(true, true);
