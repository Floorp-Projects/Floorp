if (!getBuildConfiguration().parallelJS)
    quit();

var y = Array.buildPar(9, function() {});
Array.prototype.every.call(y, (function() {
    "use asm";
    function f() {}
    return f
}))
