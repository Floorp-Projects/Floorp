if (!wasmGcEnabled() || !wasmDebuggingIsSupported()) {
    quit(0);
}

(function() {
    let g = newGlobal();
    let dbg = new Debugger(g);
    g.eval(`o = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary('(module (func (result anyref) (param anyref) get_local 0) (export "" 0))')));`);
})();

(function() {
    var g = newGlobal();
    g.parent = this;

    let src = `
      (module
        (func (export "func") (result anyref) (param $ref anyref)
            get_local $ref
        )
      )
    `;

    g.eval(`
        var obj = { somekey: 'somevalue' };

        Debugger(parent).onEnterFrame = function(frame) {
            let v = frame.environment.getVariable('var0');
            assertEq(typeof v === 'object', true);
            assertEq(typeof v.somekey === 'string', true);
            assertEq(v.somekey === 'somevalue', true);
        };

        new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(\`${src}\`))).exports.func(obj);
    `);
})();
