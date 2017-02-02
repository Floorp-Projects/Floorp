if (typeof oomTest === 'undefined')
    quit();

try {
    oomTest((function () {
        wasmTextToBinary("(module(func(loop $label1 $label0)))");
    }));
} catch(e) { }
