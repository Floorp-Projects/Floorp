// |jit-test| --fuzzing-safe; --enable-foobarbaz; error:987
// If --fuzzing-safe is used, unknown shell flags that follow it are ignored.
throw 987;
