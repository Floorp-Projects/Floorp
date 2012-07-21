// |jit-test| error: TypeError
obj = wrap(Number.bind());
Object.defineProperty(obj, "caller", {set: function () {}});
