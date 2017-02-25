for (let numLocals of [3, 4, 5, 6, 17, 18, 19]) {
    for (let numParams of [1, 2, 3, 4, 5, 6, 7, 8]) {
        let locals = `       (local `;
        let setLocals = ``;
        let getLocals = ``;
        let adds = ``;
        let sum = 0;
        for (let i = 0; i < numLocals; i++) {
            sum += i + 1;
            locals += `i32 `;
            setLocals += `       (set_local ${i + 1} (i32.add (get_local 0) (i32.const ${i + 1})))\n`;
            getLocals += `       get_local ${i + 1}\n`;
            if (i > 0)
              adds += `       i32.add\n`;
        }
        locals += `)\n`;

        var callee = `    (func $f (param `;
        var caller = `       (call $f `;
        for (let i = 0; i < numParams; i++) {
            callee += `f32 `;
            caller += `(f32.const ${i}) `;
        }
        callee += `))\n`;
        caller += `)\n`;

        var code = `(module \n` +
                   callee +
                   `    (func (export "run") (param i32) (result i32)\n` +
                   locals +
                   setLocals +
                   caller +
                   getLocals +
                   adds +
                   `    )\n` +
                   `)`;
        wasmFullPass(code, numLocals * 100 + sum, undefined, 100);
    }
}
