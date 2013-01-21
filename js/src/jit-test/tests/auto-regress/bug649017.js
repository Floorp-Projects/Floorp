// |jit-test| error:TypeError

// Binary: cache/js-dbg-64-74a8fb1bbec5-linux
// Flags:
//
try {
  throw {toString: parseInt.call};
} catch(e) { e.toString(); }
