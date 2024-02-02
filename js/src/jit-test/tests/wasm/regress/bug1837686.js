
// A bug in Ion's alias analysis was causing the two `global.get 0`s
// to be GVNd together, despite the fact that the call inside the try
// block changes the value.  In this case, this caused the returned
// value to be 7 rather than the correct value 5.

const wasm = wasmTextToBinary(`(module
  (global $g (mut i32) (i32.const 0))
  (func $f0
      i32.const 1
      global.set 0
  )
  (func $f1 (param i32) (param i32) (result i32)
    global.get 0

    i32.eqz
    if
      local.get 0
      i32.const 1
      i32.or
      local.set 0
    end

    try
        call $f0

        global.get 0
        i32.eqz
        if
            local.get 0
        i32.const 2
        i32.or
        local.set 0
        end
    catch_all
        i32.const 15
        local.set 0
    end
    local.get 0
)
  (func (export "test") (result i32)
    i32.const 4
    i32.const 0
    call $f1
  )
)`);

const mod = new WebAssembly.Module(wasm);
const ins = new WebAssembly.Instance(mod);
assertEq(ins.exports.test(), 5);
