if (typeof enableGeckoProfiling === 'undefined' || !isAsmJSCompilationAvailable())
    quit();

enableGeckoProfiling();
var code = evaluate("(function() { 'use asm'; function g() { return 43 } return g })", {
    fileName: null
});

assertEq(code()(), 43);

evaluate(`
let f = evalReturningScope.bind(null, '');

(function(glob, stdlib) {
    "use asm";
    var f = stdlib.f;
    function _() { f(); }
    return _;
})(this, { f })();
`, { fileName: null });

