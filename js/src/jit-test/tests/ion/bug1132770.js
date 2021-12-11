// |jit-test| error: too much recursion
Object.defineProperty(this, "x", {set: function() { this.x = 2; }});
setJitCompilerOption("ion.warmup.trigger", 30);
x ^= 1;
