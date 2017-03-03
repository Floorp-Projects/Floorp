// |jit-test| error: ReferenceError
for (var i = 0; i < 10; ++i) {}
for (var i = 0; i < 3; i++) {
    throw eval(raisesException);
    function ff() {}
}
