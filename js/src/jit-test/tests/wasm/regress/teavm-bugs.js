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
