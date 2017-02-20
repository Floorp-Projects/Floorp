if (typeof enableSPSProfiling === 'undefined' || !isAsmJSCompilationAvailable())
    quit();

evaluate(`
let f = evalReturningScope.bind(null, '');

(function(glob, stdlib) {
    "use asm";
    var f = stdlib.f;
    function _() { f(); }
    return _;
})(this, { f })();
`, { fileName: null });

