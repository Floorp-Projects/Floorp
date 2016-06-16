if (!('oomTest' in this))
    quit();
for (var i=0; i<2; i++)
    oomTest(() => eval("setJitCompilerOption(eval + Function, 0);"));
