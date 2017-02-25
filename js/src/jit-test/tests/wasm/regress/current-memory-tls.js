// Bug 1341650:
// - when compiled with Ion, pass the TLS register to current_memory;
// - when compiled with Baseline, don't clobber the last stack slot when
// calling into current_memory/grow_memory;

// This toy module starts with an empty memory, then tries to set values at different
// indexes, automatically growing memory when that would trigger an out of
// bounds access, for the exact right amount of pages. Note it's not made for
// efficiency, but optimizing it doesn't trigger the bugs mentioned above.

let i = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module
    (memory $mem (export "mem") 0 65535)

    (func (export "cur_mem") (result i32) (current_memory))

    (func $maybeGrow (param $i i32) (local $smem i32)
     ;; get current_memory in number of bytes, not pages.
     current_memory
     i64.extend_u/i32
     i64.const 65536
     i64.mul

     ;; get the last byte index accessed by an int32 access.
     get_local $i
     i32.const 3
     i32.add
     tee_local $i
     i64.extend_u/i32

     ;; if the memory is too small, grow it.
     i64.le_u
     if
         ;; get the floor of the accessed *page* index.
         get_local $i
         i64.extend_u/i32
         i64.const 65536
         i64.div_u

         ;; subtract to that the size of the current memory in pages;
         ;; that's the amount of pages we want to grow, minus one.
         current_memory
         i64.extend_u/i32

         i64.sub

         ;; add 1 to that amount.
         i64.const 1
         i64.add

         ;; get back to i32 and grow memory.
         i32.wrap/i64
         grow_memory
         drop
     end
    )

    (func (export "set") (param $i i32) (param $v i32)
     get_local $i
     call $maybeGrow
     get_local $i
     get_local $v
     i32.store
    )

    (func (export "get") (param $i i32) (result i32)
     get_local $i
     i32.load
    )
)
`))).exports;

assertEq(i.cur_mem(), 0);

i.set(0, 1);
assertEq(i.get(0), 1);

assertEq(i.cur_mem(), 1);

i.set(1234, 1);
assertEq(i.get(1234), 1);

assertEq(i.cur_mem(), 1);

i.set(65532, 1);
assertEq(i.get(65532), 1);

assertErrorMessage(() => i.get(65533), WebAssembly.RuntimeError, /index out of bounds/);

assertEq(i.cur_mem(), 1);

i.set(65533, 1);
assertEq(i.get(65533), 1);

assertEq(i.cur_mem(), 2);
