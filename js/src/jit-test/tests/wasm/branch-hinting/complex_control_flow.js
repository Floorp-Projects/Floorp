// Test branch hinting with nested if.

var imports = { "":{inc() { counter++ }} };
counter = 0;

let module = new WebAssembly.Module(wasmTextToBinary(`(module
  (import "" "inc" (func (result i32)))
  (func
      (result i32)
      (@metadata.code.branch_hint "\\00") (if (result i32)
          (i32.const 1)
          (then
              (@metadata.code.branch_hint "\\00") (if (result i32)
                  (i32.const 2)
                  (then
                      (@metadata.code.branch_hint "\\00") (if (result i32)
                          (i32.const 3)
                          (then
                              (@metadata.code.branch_hint "\\00") (if (result i32)
                                  (i32.const 0)
                                  (then (call 0))
                                  (else (i32.const 42))
                              )
                          )
                          (else (call 0))
                      )
                  )
                  (else (call 0))
              )
          )
          (else (call 0))
      )
  )
  (export "run" (func 1))
)`, 42, imports));

assertEq(counter, 0);
assertEq(wasmParsedBranchHints(module), true);
