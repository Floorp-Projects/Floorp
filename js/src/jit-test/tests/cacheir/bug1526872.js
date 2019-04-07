
setJitCompilerOption("offthread-compilation.enable", 0);
setJitCompilerOption("baseline.warmup.trigger", 0);
setJitCompilerOption("ion.warmup.trigger", 0);

for (var i = 0; i < 5; i++) {
    assertEq(foo(1n), false);
}

function foo(x) {
    return x == null || x == undefined;
}
