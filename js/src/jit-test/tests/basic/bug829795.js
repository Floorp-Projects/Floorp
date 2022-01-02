// |jit-test| error: TypeError

try {
    x = [];
    Array.prototype.forEach()
} catch (e) {}
x.forEach()
