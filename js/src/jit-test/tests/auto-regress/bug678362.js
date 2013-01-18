// |jit-test| error:InternalError

// Binary: cache/js-dbg-64-4b4b359e77e4-linux
// Flags:
//
var replacer = [0, 1, 2, 3];
Object.defineProperty(replacer, 3.e7, {});
JSON.stringify({ 0: 0, 1: 1, 2: 2, 3: 3 }, replacer)
