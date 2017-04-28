gczeal(4);
setJitCompilerOption("ion.warmup.trigger", 1);
function h() {
    for ([a, b] in { z: 9 }) {}
}
for (var j = 0; j < 10; j++)
    h();
