// This provokes a crash in baseline if its Stk reservation logic is not up to
// snuff, bug 1675844.

var txt = `
(module
  (type (;0;) (func (result f32 f32 i32)))
  (func $main (type 0) (result f32 f32 i32)
    call $main
    call $main
    call $main
    call $main
    call $main
    call $main
    call $main
    call $main
    call $main
    call $main
    call $main
    call $main
    call $main
    call $main
    call $main
    call $main
    call $main
    call $main
    call $main
    memory.size
    call $main
    call $main
    call $main
    call_indirect (type 0)
    call $main
    call $main
    call $main
    call $main
    call $main
    call $main
    memory.size
    call $main
    call $main
    call $main
    call_indirect (type 0)
    call $main
    call $main
    call $main
    call $main
    call $main
    call $main
    call $main
    call $main
    call $main
    call $main
    call $main
    call $main
    memory.size)
  (table (;0;) 62 255 funcref)
  (memory (;0;) 15 18)
  (export "t1" (table 0))
  (export "memory" (memory 0)))`;
assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(txt)),
                   WebAssembly.CompileError,
                   /(unused values not explicitly dropped)|(expected f32, found i32)/);

