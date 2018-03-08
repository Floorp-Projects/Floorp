if (getJitCompilerOptions()["ion.warmup.trigger"] > 30)
    setJitCompilerOption("ion.warmup.trigger", 30);
function Iterate(items) {
    for (var value of items) {}
}
var iterable = {
    *[Symbol.iterator]() {
        return "pass";
        (yield* iterable);
    }
};
for (var i = 0; i < 20; ++i) {
    Iterate(iterable);
}
