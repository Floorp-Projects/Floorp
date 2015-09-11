f = (function() {
    "use asm";
    var a;
    function f() {
        var b = -1;
    }
    return f;
})();
for (var j = 0; j < 1; ++j)
    f();
setJitCompilerOption('ion.forceinlineCaches', 1);
Math.fround(
    Math.fround()
);
for (var j = 0; j < 1; ++j)
    (function() {})();
