// |jit-test| error: Error

var g = newGlobal();
g.f = setJitCompilerOption;
g.eval("clone(f)()(9)")
