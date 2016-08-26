// |jit-test| exitstatus: 6;

setJitCompilerOption('ion.interrupt-without-signals', 1);
timeout(1);
for(;;);
