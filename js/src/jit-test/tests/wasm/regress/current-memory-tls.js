// Bug 1341650:
// - when compiled with Ion, pass the TLS register to memory.size;
// - when compiled with Baseline, don't clobber the last stack slot when
// calling into memory.size/memory.grow;

// This toy module starts with an empty memory, then tries to set values at different
// indexes, automatically growing memory when that would trigger an out of
// bounds access, for the exact right amount of pages. Note it's not made for
// efficiency, but optimizing it doesn't trigger the bugs mentioned above.

let i = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`
(module
    (memory $mem (export "mem") 0 65535)

    (func (export "cur_mem") (result i32) (memory.size))

    (func $maybeGrow (param $i i32) (local $smem i32)
     ;; get memory.size in number of bytes, not pages.
     memory.size
     i64.extend_i32_u
     i64.const 65536
     i64.mul

     ;; get the last byte index accessed by an int32 access.
     local.get $i
     i32.const 3
     i32.add
     local.tee $i
     i64.extend_i32_u

     ;; if the memory is too small, grow it.
     i64.le_u
     if
         ;; get the floor of the accessed *page* index.
         local.get $i
         i64.extend_i32_u
         i64.const 65536
         i64.div_u

         ;; subtract to that the size of the current memory in pages;
         ;; that's the amount of pages we want to grow, minus one.
         memory.size
         i64.extend_i32_u

         i64.sub

         ;; add 1 to that amount.
         i64.const 1
         i64.add

         ;; get back to i32 and grow memory.
         i32.wrap_i64
         memory.grow
         drop
     end
    )

    (func (export "set") (param $i i32) (param $v i32)
     local.get $i
     call $maybeGrow
     local.get $i
     local.get $v
     i32.store
    )

    (func (export "get") (param $i i32) (result i32)
     local.get $i
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
