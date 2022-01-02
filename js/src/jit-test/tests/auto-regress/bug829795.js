// |jit-test| error:TypeError

// Binary: cache/js-dbg-64-44dcffe8792b-linux
// Flags: -a
//
try {
    x = [];
    Array.prototype.forEach()
} catch (e) {}
x.forEach()
