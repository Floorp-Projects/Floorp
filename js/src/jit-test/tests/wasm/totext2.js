// |jit-test| test-also-wasm-baseline
if (!wasmIsSupported())
     quit();

load(libdir + "asserts.js");

function runTest(code, expected) {
  var binary = wasmTextToBinary(code);
  var s = wasmBinaryToText(binary, "experimental");
  s = s.replace(/\s+/g, ' ');
  print("TEXT: " + s);
  assertEq(expected, s);
}

// Smoke test
runTest(`
(module
  (func (param i32) (result f64)
     (local $l f32)
     (block
        (set_local $l (f32.const 0.0))
        (loop $exit $cont
           (br_if $exit (get_local 0))
           (br 2)
        )
        (if (i32.const 1)
           (f64.min (f64.neg (f64.const 1)) (f64.const 0))
           (f64.add (f64.const 0.5) (f64.load offset=0 (i32.const 0)) )
        )
     )
     (i32.store16 (i32.const 8) (i32.const 128))

     (return (f64.const 0))
  )
  (export "test" 0)
  (memory 1 10)
)`,
"type $type0 of function (i32) : (f64); " +
"export $func0 as \"test\"; " +
"function $func0($var0: i32) : (f64) {" +
" var $var1: f32; { $var1 = 0.0f; loop { br_if ($var0) $label0; br $label1; $label0: };" +
" if (1) { f64.min(-1.0, 0.0) } else { 0.5 + f64[0] } $label1: };" +
" i32:16[8] = 128; return 0.0; "+
"} memory 1, 10 {} ");

// function calls
runTest(`
(module
  (type $type1 (func (param i32) (result i32)))
  (import $import1 "mod" "test" (param f32) (result f32))
  (table $func1 $func2)
  (func $func1 (param i32) (param f32) (nop))
  (func $func2 (param i32) (result i32) (get_local 0))
  (func $test
    (call $func1
      (call_indirect $type1 (i32.const 1) (i32.const 2))
      (call_import $import1 (f32.const 1.0))
    )
  )
  (export "test" $test)
  (memory 1 65535)
)`,
"type $type0 of function (i32) : (i32); " +
"type $type1 of function (f32) : (f32); " +
"type $type2 of function (i32, f32) : (); " +
"type $type3 of function () : (); " +
"import \"test\" as $import0 from \"mod\" typeof function (f32) : (f32); " +
"table [$func0, $func1]; export $func2 as \"test\"; " +
"function $func0($var0: i32, $var1: f32) : () { nop; } " +
"function $func1($var0: i32) : (i32) { $var0 } " +
"function $func2() : () {" +
" call $func0 (call_indirect $type0 [1] (2), call_import $import0 (1.0f)) " +
"} memory 1, 65535 {} ");

// precedence
runTest(`
(module
  (func $test
    (local $0 i32) (local $1 f64)
    (i32.add (i32.mul (i32.mul (i32.const 1) (i32.const 2)) (i32.const 3))
             (i32.mul (i32.const 4)  (i32.mul (i32.const 5) (i32.const 6))))
    (i32.mul (i32.add (i32.add (i32.const 1) (i32.const 2)) (i32.const 3))
             (i32.add (i32.const 4)  (i32.add (i32.const 5) (i32.const 6))))
    (i32.add (i32.const 0) (i32.sub (i32.const 1) (i32.mul (i32.const 2) (i32.div_s (i32.const 3) (i32.const 4)))))
    (i32.sub (i32.add (i32.const 0) (i32.const 1)) (i32.div_s (i32.mul (i32.const 2) (i32.const 3)) (i32.const 4)))
    (set_local $0 (i32.store8 (i32.const 8) (i32.or (i32.const 0)
      (i32.xor (i32.const 1) (i32.and (i32.const 2) (i32.eq (i32.const 3)
      (i32.lt_u (i32.const 4) (i32.shr_u (i32.const 5) (i32.add (i32.const 6)
      (i32.mul (i32.const 7) (i32.eqz (i32.load16_u (get_local $0)))))))))))))
    (f64.load (i32.trunc_u/f64 (f64.neg (f64.mul (f64.const 0.0)
      (f64.add (f64.const 1.0) (f64.convert_s/i32 (i32.lt_s (i32.const 6)
      (f64.eq (f64.const 7.0) (f64.store (i32.const 8) (set_local $1 (f64.const 9.0)))))))))))
    (unreachable)
  )
  (export "test" $test)
  (memory 0 0)
)`,
"type $type0 of function () : (); " +
"export $func0 as \"test\"; " +
"function $func0() : () {" +
" var $var0: i32, $var1: f64;" +
" 1 * 2 * 3 + 4 * (5 * 6);" +
" (1 + 2 + 3) * (4 + (5 + 6));" +
" 0 + (1 - 2 * (3 /s 4));" +
" 0 + 1 - 2 * 3 /s 4;" +
" $var0 = i32:8[8] = 0 | 1 ^ 2 & 3 == 4 <u 5 >>u 6 + 7 * !i32:16u[$var0];" +
" f64[i32.trunc_u/f64(-(0.0 * (1.0 + f64.convert_s/i32(6 <s (7.0 == (f64[8] = $var1 = 9.0))))))];" +
" unreachable; " +
"} memory 0, 0 {} ");
