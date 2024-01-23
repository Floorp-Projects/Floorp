wasmEvalText(`(module
    (func)
    (start 0)
    (table $0 1 funcref)
    (elem 0 (i32.const 0) func 0)
)`);
