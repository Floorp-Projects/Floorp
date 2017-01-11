if (typeof enableSPSProfiling === 'undefined' || !isAsmJSCompilationAvailable())
    quit();

enableSPSProfiling();
var code = evaluate("(function() { 'use asm'; function g() { return 43 } return g })", {
    fileName: null
});

assertEq(code()(), 43);
