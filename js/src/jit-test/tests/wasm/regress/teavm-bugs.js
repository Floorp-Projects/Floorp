// Register allocation issue with LCompareI64AndBranch.
let params = '';
let locals = '';
let tests = '(i64.const 0)';

for (let i = 15; i --> 0;) {
    params += `\n(param i64)`;
    locals += `\n(local i64)`;
    tests = `
    (if (result i64)
        (i64.eq
            (local.get ${i + 8})
            (local.get ${i})
        )
        (then (local.get ${i + 8}))
        (else ${tests})
    )`;
}

let code = `(module
   (func $i64
     ${params} (result i64) ${locals}
     ${tests}
   )
)`

wasmEvalText(code);

// Bounds check elimination.
assertEq(wasmEvalText(`(module
    (memory 1)
    (func (param $p i32) (result i32) (local $l i32)
        (local.set $l (i32.const 0))
        (if
            (local.get $p)
            (then (local.set $l
               (i32.add
                  (local.get $l)
                  (i32.load8_s (local.get $p))
               )
            ))
        )
        (local.set $l
           (i32.add
              (local.get $l)
              (i32.load8_s (local.get $p))
           )
        )
        (local.get $l)
    )
    (data (i32.const 0) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f")
    (export "test" (func 0))
)`).exports["test"](3), 6);
