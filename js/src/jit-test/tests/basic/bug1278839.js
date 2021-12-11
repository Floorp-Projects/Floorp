// |jit-test| skip-if: !('oomTest' in this)
for (var i=0; i<2; i++)
    oomTest(() => eval("setJitCompilerOption(eval + Function, 0);"));
