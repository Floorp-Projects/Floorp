if (typeof oomAfterAllocations !== 'function' || typeof evaluate !== 'function')
    quit();

oomAfterAllocations(10, 2);
evaluate(`function mod(stdlib, ffi, heap) {
    "use asm";
    function f3(k) {
        k = k | 0;
    }
    function g3(k) {}
}`);
