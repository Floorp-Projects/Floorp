// |jit-test| test-also-wasm-baseline
load(libdir + "wasm.js");

// Register allocation issue with LCompareI64AndBranch.
let params = '';
let locals = '';
let tests = '';

for (let i = 15; i --> 0;) {
    params += `\n(param i64)`;
    locals += `\n(local i64)`;
    tests = `
    (if
        (i64.eq
            (get_local ${i + 8})
            (get_local ${i})
        )
        (get_local ${i + 8})
        ${tests}
    )`;
}

let code = `(module
   (func $i64
     ${params} ${locals}
     ${tests}
   )
)`

evalText(code);

// Bounds check elimination.
assertEq(evalText(`(module
    (memory 1)
    (func (param $p i32) (local $l i32) (result i32)
        (set_local $l (i32.const 0))
        (if
            (get_local $p)
            (set_local $l
               (i32.add
                  (get_local $l)
                  (i32.load8_s (get_local $p))
               )
            )
        )
        (set_local $l
           (i32.add
              (get_local $l)
              (i32.load8_s (get_local $p))
           )
        )
        (get_local $l)
    )
    (data 0 "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f")
    (export "test" 0)
)`).exports["test"](3), 6);
