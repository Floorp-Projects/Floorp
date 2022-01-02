// |jit-test| error: ReferenceError

gczeal(4, 10);
f = ([a = class target extends b {}, b] = [void 0]) => {
    class dbg {}
    class get {}
};
f()
