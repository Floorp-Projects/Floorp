
setJitCompilerOption("offthread-compilation.enable", 0);
setJitCompilerOption("baseline.warmup.trigger", 0);
setJitCompilerOption("ion.warmup.trigger", 0);

foo();

function foo() {
    Array.prototype.__proto__ = null;
    Array.prototype[1] = 'bar';
}
