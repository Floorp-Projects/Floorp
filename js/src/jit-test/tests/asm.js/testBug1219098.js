load(libdir + "asm.js");

var bigScript = `
    function wee() { return 42 }
    function asmModule() { 'use asm'; function f() { return 43 } return f}
` + ' '.repeat(10 * 1024 * 1024);

eval(bigScript);

if (isAsmJSCompilationAvailable())
    assertEq(isAsmJSModule(asmModule), true);

assertEq(wee(), 42);
assertEq(eval('(' + wee.toSource() + ')')(), 42);
