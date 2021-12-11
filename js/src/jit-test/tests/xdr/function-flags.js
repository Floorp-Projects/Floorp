load(libdir + 'bytecode-cache.js');

var test;

// Ensure that if a function is encoded we don't encode its "name
// resolved" flag.
test = `
  function f() { delete f.name; return f.hasOwnProperty('name'); }
  f();
  `
evalWithCache(test, { assertEqBytecode: true, assertEqResult: true });

test = `
  function f() { return f.hasOwnProperty('name'); }
  f();
  `
evalWithCache(test, { assertEqBytecode: true, assertEqResult: true });

// Ensure that if a function is encoded we don't encode its "length
// resolved" flag.
test = `
  function f() { delete f.length; return f.hasOwnProperty('length'); }
  f();
  `
evalWithCache(test, { assertEqBytecode: true, assertEqResult: true });

test = `
  function f() { return f.hasOwnProperty('length'); }
  f();
  `
evalWithCache(test, { assertEqBytecode: true, assertEqResult: true });

// And make sure our bytecode is actually not reflecting the flags,
// not that we ignore them on decode.
test = `
  function f() { return f.hasOwnProperty('length') || f.hasOwnProperty('name'); }
  f();
  `
evalWithCache(test, { assertEqBytecode: true, assertEqResult: true });

