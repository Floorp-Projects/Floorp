enableSPSProfiling();
for (var j = 0; j < 1000; ++j) {
  (function(stdlib) {
    "use asm";
    var pow = stdlib.Math.pow;
    function f() {
        return +pow(.0, .0)
    }
    return f;
})(this)()
}
