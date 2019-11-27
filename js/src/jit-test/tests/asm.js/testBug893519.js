// |jit-test| error:Error: AsmJS modules do not yet support cloning; skip-if: !isAsmJSCompilationAvailable()

var g = newGlobal({newCompartment: true});
cloneAndExecuteScript("function h() { function f() { 'use asm'; function g() { return 42 } return g } return f }", g);
