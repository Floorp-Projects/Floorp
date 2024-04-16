// Make sure we are correctly parsing this custom section.
var code =`
(module
    (func $$dummy)
    (func $main (param i32) (result i32)
        i32.const 0
        local.get 0
        i32.eq
        ;; Only allowed on br_if and if
        (@metadata.code.branch_hint "\\00") if
            call $$dummy
            i32.const 1
            return
        else
            call $$dummy
            i32.const 0
            return
        end
        i32.const 3
        return
    )

    (export "_main" (func $main))
)`;

let branchHintsModule = new WebAssembly.Module(wasmTextToBinary(code));
assertEq(WebAssembly.Module.customSections(branchHintsModule, "metadata.code.branch_hint").length, 1);
assertEq(wasmParsedBranchHints(branchHintsModule), true);

let instance = new WebAssembly.Instance(branchHintsModule);
assertEq(instance.exports._main(0), 1);

// Testing branch hints parsing on `if` and `br_if`
branchHintsModule = new WebAssembly.Module(wasmTextToBinary(`
(module
    (func $main
      i32.const 0
      (@metadata.code.branch_hint "\\00")
      if
        i32.const 0
        (@metadata.code.branch_hint "\\01")
        br_if 0
      end
    )
    (export "_main" (func $main))
)`));
assertEq(wasmParsedBranchHints(branchHintsModule), true);
instance = new WebAssembly.Instance(branchHintsModule);
instance.exports._main();

let m = new WebAssembly.Module(wasmTextToBinary(`
(module
  (type (;0;) (func))
  (type (;1;) (func (param i32) (result i32)))
  (type (;2;) (func (result i32)))
  (func $__wasm_nullptr (type 0)
    unreachable)
  (func $main (type 2) (result i32)
    (local i32 i32 i32 i32)
    i32.const 0
    local.tee 2
    local.set 3
    loop
      local.get 2
      i32.const 50000
      i32.eq
      (@metadata.code.branch_hint "\\00") if
        i32.const 1
        local.set 3
      end
      local.get 2
      i32.const 1
      i32.add
      local.tee 2
      i32.const 100000
      i32.ne
      (@metadata.code.branch_hint "\\01") br_if 0 (;@1;)
    end
    local.get 3)
  (table (;0;) 1 1 funcref)
  (memory (;0;) 17 128)
  (global (;0;) (mut i32) (i32.const 42))
  (export "memory" (memory 0))
  (export "_main" (func $main))
  (elem (;0;) (i32.const 0) func $__wasm_nullptr)
  (type (;0;) (func (param i32)))
)`));

assertEq(wasmParsedBranchHints(m), true);
instance = new WebAssembly.Instance(m);
assertEq(instance.exports._main(0), 1);

// Testing invalid values for branch hints
assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(`
(module
    (func $main (param i32) (result i32)
      i32.const 0
      (@metadata.code.branch_hint "\\0000000") if
        i32.const 1
        return
      end
      i32.const 42
      return
    )
  )
`)), SyntaxError, /invalid value for branch hint/);

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(`
(module
    (func $main (param i32) (result i32)
      i32.const 0
      (@metadata.code.branch_hint "\\02") if
        i32.const 1
        return
      end
      i32.const 42
      return
    )
  )
`)), SyntaxError, /invalid value for branch hint/);

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(`
(module
    (func $main (param i32) (result i32)
      i32.const 0
      (@metadata.code.branch_hint "\\aaaa") if
        i32.const 1
        return
      end
      i32.const 42
      return
    )
  )
`)), SyntaxError, /wasm text error/);

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(`
(module
    (func $main (param i32) (result i32)
      i32.const 0
      (@metadata.code.branch_hint) if
        i32.const 1
        return
      end
      i32.const 42
      return
    )
  )
`)), SyntaxError, /wasm text error/);

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(`
(module
    (@metadata.code.branch_hint)
)
`)), SyntaxError, /wasm text error/);
