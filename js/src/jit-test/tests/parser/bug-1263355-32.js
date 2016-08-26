// |jit-test| error: ReferenceError

f = ([a = class target extends b {}, b] = [void 0]) => {};
f()
