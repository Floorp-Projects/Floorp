// This is intended to test that Ion correctly handles the combination
// {direct, indirect} x {i32, i64} for global variables, following GVN
// improvements pertaining to globals in bug 1448277.

let txt =
  `(module
     ;; -------- Globals --------
     (global $ind64 (export "ind64") (mut i64) (i64.const 4242))
     (global $ind32 (export "ind32") (mut i32) (i32.const 3141))
     (global $dir64 (mut i64) (i64.const 5927))
     (global $dir32 (mut i32) (i32.const 2718))
     ;; -------- FUNCTION 0 --------
     (func (export "function0") (result i32)
       (local $loopctr i32)
       (local $sum     i32)
       (local $tmp64   i64)
       (local $tmp32   i32)
       (loop
          (global.set $ind64 (i64.add (global.get $ind64) (i64.const 11)))
          (global.set $ind32 (i32.add (global.get $ind32) (i32.const 22)))
          (global.set $dir64 (i64.add (global.get $dir64) (i64.const 33)))
          (global.set $dir32 (i32.add (global.get $dir32) (i32.const 44)))

          (local.set $tmp64 (i64.and (global.get $ind64) (global.get $dir64)))
          (local.set $tmp32 (i32.or  (global.get $ind32) (global.get $dir32)))

          (local.set $sum
            (i32.sub (local.get $sum) (i32.xor (i32.wrap_i64 (local.get $tmp64))
                                               (local.get $tmp32))))

          (local.set $loopctr
            (i32.add (local.get $loopctr) (i32.const 1)))
          (br_if 0
            (i32.lt_u (local.get $loopctr) (i32.const 10)))

       )
       (local.get $sum)
     )
   )`;

function test_driver()
{
    let bin  = wasmTextToBinary(txt);
    let inst = new WebAssembly.Instance(new WebAssembly.Module(bin));
    let res  = inst.exports.function0();
    assertEq(res, -79170);
}

test_driver();
