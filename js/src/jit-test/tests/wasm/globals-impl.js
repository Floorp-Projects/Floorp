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
          (set_global $ind64 (i64.add (get_global $ind64) (i64.const 11)))
          (set_global $ind32 (i32.add (get_global $ind32) (i32.const 22)))
          (set_global $dir64 (i64.add (get_global $dir64) (i64.const 33)))
          (set_global $dir32 (i32.add (get_global $dir32) (i32.const 44)))

          (set_local $tmp64 (i64.and (get_global $ind64) (get_global $dir64)))
          (set_local $tmp32 (i32.or  (get_global $ind32) (get_global $dir32)))

          (set_local $sum
            (i32.sub (get_local $sum) (i32.xor (i32.wrap/i64 (get_local $tmp64))
                                               (get_local $tmp32))))

          (set_local $loopctr
            (i32.add (get_local $loopctr) (i32.const 1)))
          (br_if 0
            (i32.lt_u (get_local $loopctr) (i32.const 10)))

       )
       (get_local $sum)
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
