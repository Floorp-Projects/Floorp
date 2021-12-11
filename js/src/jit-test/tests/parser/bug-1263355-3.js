// |jit-test| error: ReferenceError

f = ([a = class b extends b {}, b] = [void 0]) => {};
f()
