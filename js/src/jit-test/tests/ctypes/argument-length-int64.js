load(libdir + 'asserts.js');

function test() {
  assertTypeErrorMessage(() => { ctypes.Int64(1).toString(1, 2); },
                         "Int64.prototype.toString takes at most one argument");
  assertTypeErrorMessage(() => { ctypes.Int64(1).toSource(1); },
                         "Int64.prototype.toSource takes no arguments");
  assertTypeErrorMessage(() => { ctypes.Int64(); },
                         "Int64 constructor takes one argument");
  assertTypeErrorMessage(() => { ctypes.Int64.compare(); },
                         "Int64.compare takes two arguments");
  assertTypeErrorMessage(() => { ctypes.Int64.lo(); },
                         "Int64.lo takes one argument");
  assertTypeErrorMessage(() => { ctypes.Int64.hi(); },
                         "Int64.hi takes one argument");
  assertTypeErrorMessage(() => { ctypes.Int64.join(); },
                         "Int64.join takes two arguments");

  assertTypeErrorMessage(() => { ctypes.UInt64(1).toString(1, 2); },
                         "UInt64.prototype.toString takes at most one argument");
  assertTypeErrorMessage(() => { ctypes.UInt64(1).toSource(1); },
                         "UInt64.prototype.toSource takes no arguments");
  assertTypeErrorMessage(() => { ctypes.UInt64(); },
                         "UInt64 constructor takes one argument");
  assertTypeErrorMessage(() => { ctypes.UInt64.compare(); },
                         "UInt64.compare takes two arguments");
  assertTypeErrorMessage(() => { ctypes.UInt64.lo(); },
                         "UInt64.lo takes one argument");
  assertTypeErrorMessage(() => { ctypes.UInt64.hi(); },
                         "UInt64.hi takes one argument");
  assertTypeErrorMessage(() => { ctypes.UInt64.join(); },
                         "UInt64.join takes two arguments");
}

if (typeof ctypes === "object")
  test();
