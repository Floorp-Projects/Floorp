function surprise(depth) {
    arguments.callee.caller(depth);
}

(function(depth) {
    function foo() { function asmModule() { 'use asm'; return {} } };
    if (depth)
        surprise(depth - 1);
})(2);
