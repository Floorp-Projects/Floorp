// |jit-test| error: TypeError
x = {};
Array.prototype.push.call(x, 0);
Object.freeze(x);
Array.prototype.unshift.call(x, 0);
