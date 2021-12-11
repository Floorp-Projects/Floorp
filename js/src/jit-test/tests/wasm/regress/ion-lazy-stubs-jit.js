(function coerceinplace() {
    var { table } = wasmEvalText(`(module
        (func $add (param i32) (param i32) (result i32)
         local.get 0
        )
        (table (export "table") 10 funcref)
        (elem (i32.const 0) $add)
    )`).exports;

    for (var i = 0; i < 100; i++) {
      table.get(0)((true).get++, i*2+1);
    }
})();
