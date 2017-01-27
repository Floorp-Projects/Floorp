if (typeof enableGeckoProfiling === 'undefined' || !isAsmJSCompilationAvailable())
    quit();

enableGeckoProfiling();
var code = evaluate("(function() { 'use asm'; function g() { return 43 } return g })", {
    fileName: null
});

assertEq(code()(), 43);
