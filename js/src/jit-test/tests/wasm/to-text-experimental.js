load(libdir + "wasm.js");

assertErrorMessage(() => wasmBinaryToText(wasmTextToBinary(`(module (func (result i32) (f32.const 13.37)))`), 'experimental'), WebAssembly.CompileError, /type mismatch/);

function runTest(code, expected) {
  var binary = wasmTextToBinary(code);
  var s = wasmBinaryToText(binary, "experimental");
  s = s.replace(/\s+/g, ' ');
  print("TEXT: " + s);
  assertEq(s, expected);
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
        (drop (if f64 (i32.const 1)
           (f64.min (f64.neg (f64.const 1)) (f64.const 0))
           (f64.add (f64.const 0.5) (f64.load offset=0 (i32.const 0)) )
        ))
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
" var $var1: f32; { $var1 = 0.0f; { loop { br_if ($var0) $label0; br $label1; } $label0: };" +
" if (1) { f64.min(-1.0, 0.0) } else { 0.5 + f64[0] }; $label1: };" +
" i32:16[8] = 128; return 0.0; "+
"} memory 1, 10 {} ");

wasmFailValidateText(
`(module
  (func (param i32) (result f64)
     (local $l f32)
     (block
        (set_local $l (f32.const 0.0))
        (loop $cont
           (br_if $cont (get_local 0))
           (br 2)
        )
        (drop (if f64 (i32.const 1)
           (f64.min (f64.neg (f64.const 1)) (f64.const 0))
           (f64.add (f64.const 0.5) (f64.load offset=0 (i32.const 0)) )
        ))
     )
     (i32.store16 (i32.const 8) (i32.const 128))

     (return (f64.const 0))
  )
  (export "test" 0)
  (memory 1 10)
)`, /popping value from empty stack/);

// function calls
runTest(`
(module
  (type $type1 (func (param i32) (result i32)))
  (import $import1 "mod" "test" (param f32) (result f32))
  (table anyfunc (elem $func1 $func2))
  (func $func1 (param i32) (param f32) (nop))
  (func $func2 (param i32) (result i32) (get_local 0))
  (func $test
    (call $func1
      (call_indirect $type1 (i32.const 2) (i32.const 1))
      (call $import1 (f32.const 1.0))
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
"table [$func1, $func2]; export $func3 as \"test\"; " +
"function $func1($var0: i32, $var1: f32) : () { nop; } " +
"function $func2($var0: i32) : (i32) { $var0 } " +
"function $func3() : () {" +
" call $func1 (call_indirect $type0 (2) [1], call $import0 (1.0f)) " +
"} memory 1, 65535 {} ");

// precedence
runTest(`
(module
  (func $test
    (local $0 i32) (local $1 f64) (local $2 f64)
    (drop (i32.add (i32.mul (i32.mul (i32.const 1) (i32.const 2)) (i32.const 3))
                   (i32.mul (i32.const 4)  (i32.mul (i32.const 5) (i32.const 6)))))
    (drop (i32.mul (i32.add (i32.add (i32.const 1) (i32.const 2)) (i32.const 3))
                   (i32.add (i32.const 4)  (i32.add (i32.const 5) (i32.const 6)))))
    (drop (i32.add (i32.const 0) (i32.sub (i32.const 1) (i32.mul (i32.const 2) (i32.div_s (i32.const 3) (i32.const 4))))))
    (drop (i32.sub (i32.add (i32.const 0) (i32.const 1)) (i32.div_s (i32.mul (i32.const 2) (i32.const 3)) (i32.const 4))))
    (i32.store8 (i32.const 8) (i32.or (i32.const 0)
      (i32.xor (i32.const 1) (i32.and (i32.const 2) (i32.eq (i32.const 3)
      (i32.lt_u (i32.const 4) (i32.shr_u (i32.const 5) (i32.add (i32.const 6)
      (i32.mul (i32.const 7) (i32.eqz (i32.load16_u (get_local $0))))))))))))
    (drop (f64.load (i32.trunc_u/f64 (f64.neg (f64.mul (f64.const 0.0)
      (f64.add (f64.const 1.0) (f64.convert_s/i32 (i32.lt_s (i32.const 6)
      (f64.eq (f64.const 7.0) (tee_local $2 (tee_local $1 (f64.const 9.0))))))))))))
    (f64.store (i32.const 8) (tee_local $1 (f64.const 9.0)))
    (unreachable)
  )
  (export "test" $test)
  (memory 0 0)
)`,
"type $type0 of function () : (); " +
"export $func0 as \"test\"; " +
"function $func0() : () {" +
" var $var0: i32, $var1: f64, $var2: f64;" +
" 1 * 2 * 3 + 4 * (5 * 6);" +
" (1 + 2 + 3) * (4 + (5 + 6));" +
" 0 + (1 - 2 * (3 /s 4));" +
" 0 + 1 - 2 * 3 /s 4;" +
" i32:8[8] = 0 | 1 ^ 2 & 3 == 4 <u 5 >>u 6 + 7 * !i32:16u[$var0];" +
" f64[i32.trunc_u/f64(-(0.0 * (1.0 + f64.convert_s/i32(6 <s (7.0 == ($var2 = $var1 = 9.0))))))];" +
" f64[8] = $var1 = 9.0;" +
" unreachable; " +
"} memory 0, 0 {} ");

// more stack-machine code that isn't directly representable as an AST
runTest(`
(module
   (func (result i32)
     (local $x i32)
     i32.const 100
     set_local $x
     i32.const 200
     set_local $x
     i32.const 400
     set_local $x
     i32.const 2
     i32.const 16
     nop
     set_local $x
     i32.const 3
     i32.const 17
     set_local $x
     i32.const 18
     set_local $x
     i32.lt_s
     if i32
       i32.const 101
       set_local $x
       i32.const 8
       i32.const 102
       set_local $x
     else
       i32.const 103
       set_local $x
       i32.const 900
       i32.const 104
       set_local $x
       i32.const 105
       set_local $x
     end
     i32.const 107
     set_local $x
     get_local $x
     i32.add
     i32.const 106
     set_local $x
   )
   (export "" 0)
)`,
"type $type0 of function () : (i32); export $func0 as \"\"; function $func0() : (i32) { var $var0: i32; $var0 = 100; $var0 = 200; $var0 = 400; first(first(if (first(2, ($var0 = first(16, nop))) <s first(3, ($var0 = 17), ($var0 = 18))) { $var0 = 101; first(8, $var0 = 102) } else { $var0 = 103; first(900, $var0 = 104, $var0 = 105) }, ($var0 = 107)) + $var0, $var0 = 106) } ");

// more stack-machine code that isn't directly representable as an AST
runTest(`
(module
   (func $return_void)

   (func (result i32)
     (local $x i32)
     i32.const 0
     block
       i32.const 1
       set_local $x
     end
     i32.const 2
     set_local $x
     i32.const 3
     loop
       i32.const 4
       set_local $x
     end
     i32.const 5
     set_local $x
     i32.add
     call $return_void
   )
   (export "" 0)
)`,
"type $type0 of function () : (); type $type1 of function () : (i32); export $func0 as \"\"; function $func0() : () { } function $func1() : (i32) { var $var0: i32; first(first(0, { $var0 = 1; }, ($var0 = 2)) + first(3, loop { $var0 = 4; }, ($var0 = 5)), call $func0 ()) } ");

runTest(`
(module
  (func $func
    block $block
      i32.const 0
      if
        i32.const 0
        if
        end
      else
      end
    end
  )
  (export "" 0)
)`,
"type $type0 of function () : (); export $func0 as \"\"; function $func0() : () { { if (0) { if (0) { }; }; } } ");
