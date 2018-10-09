// |jit-test| skip-if: !('oomTest' in this)

try {
    oomTest((function () {
        wasmTextToBinary("(module(func(loop $label1 $label0)))");
    }));
} catch(e) { }
