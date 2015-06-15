// |jit-test| --no-baseline; --non-writable-jitcode
(function(stdlib, foreign, heap) {
    "use asm";
    function f() {}
    return f;
})();
