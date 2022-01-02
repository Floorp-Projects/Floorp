// |jit-test| --ion-eager; --no-threads
setJitCompilerOption("ion.forceinlineCaches", 1);
"".localeCompare();
