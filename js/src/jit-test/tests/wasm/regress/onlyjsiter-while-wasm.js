if (typeof evalInCooperativeThread === 'undefined')
    quit();

try {
    evalInCooperativeThread(`
        var { f } = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(\`
        (module
         (func $f (export "f") call $f)
        )
        \`))).exports;
        gczeal(9);
        f();
    `);
} catch(e) {}
