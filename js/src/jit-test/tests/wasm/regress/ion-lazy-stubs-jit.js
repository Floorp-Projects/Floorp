(function coerceinplace() {
    var { table } = wasmEvalText(`(module
        (func $add (result i32) (param i32) (param i32)
         get_local 0
        )
        (table (export "table") 10 anyfunc)
        (elem (i32.const 0) $add)
    )`).exports;

    for (var i = 0; i < 100; i++) {
      table.get(0)((true).get++, i*2+1);
    }
})();

(function reporti64() {
    var instance = wasmEvalText(`(module
        (func $add (export "add") (result i32) (param i32) (param i32)
            get_local 0
            get_local 1
            i32.add
        )

        (func $addi64 (result i64) (param i32) (param i32)
            get_local 0
            get_local 1
            call $add
            i64.extend_s/i32
        )

        (table (export "table") 10 anyfunc)
        (elem (i32.const 0) $add $addi64)
    )`).exports;

    const EXCEPTION_ITER = 50;

    for (var i = 0; i < 100; i++) {
        var caught = null;

        var arr = [instance.add, (x,y)=>x+y];
        if (i === EXCEPTION_ITER) {
            arr[0] = instance.table.get(1);
        } else if (i === EXCEPTION_ITER + 1) {
            arr[0] = instance.add;
        }

        try {
            arr[i%2](i, i);
        } catch(e) {
            caught = e;
            print(e);
        }

        assertEq(!!caught, i === EXCEPTION_ITER);
    }
})();
