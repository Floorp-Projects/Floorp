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
"type $type0 of function (i32) : (f64) " +
"export $func0 as \"test\" " +
"function $func0($var0:i32) : (f64) {" +
" var $var1:f32 { $var1 = 0.0f loop { br_if $var0,$label0 br $label1 $label0: }" +
" if (1) { f64.min -1.0 0.0 } else { 0.5 + f64.load [0] } $label1: }" +
" i32.store16 [8],128 return 0.0 "+
"} memory 1,10 {} ");

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
"type $type0 of function (i32) : (i32) " +
"type $type1 of function (f32) : (f32) " +
"type $type2 of function (i32,f32) : () " +
"type $type3 of function () : () " +
"import \"test\" as $import0 from \"mod\" typeof function (f32) : (f32) " +
"table [$func0,$func1] export $func2 as \"test\" " +
"function $func0($var0:i32,$var1:f32) : () { nop } " +
"function $func1($var0:i32) : (i32) { $var0 } " +
"function $func2() : () {" +
" call $func0 (call_indirect $type0 [1] (2),call_import $import0 (1.0f)) " +
"} memory 1,65535 {} ");
