// |jit-test| error: ReferenceError: reference to undefined property e.stack
options("strict", "werror");

try {
    throw 5;
} catch(e) {
    print(e + ',' + e.stack);
}
