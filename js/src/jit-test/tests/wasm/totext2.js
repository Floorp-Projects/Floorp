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
"type $type$0 of function (i32) : (f64) " +
"export $func$0 as \"test\" " +
"function $func$0($var$0:i32) : (f64) {" +
" var $var$1:f32 { $var$1 = 0.0f loop { br_if $var$0,$label$0 br $label$1 $label$0: }" +
" if (1) { f64.min -1.0 0.0 } else { 0.5 + f64.load [0] } $label$1: }" +
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
"type $type$0 of function (i32) : (i32) " +
"type $type$1 of function (f32) : (f32) " +
"type $type$2 of function (i32,f32) : () " +
"type $type$3 of function () : () " +
"import \"test\" as $import$0 from \"mod\" typeof function (f32) : (f32) " +
"table [$func$0,$func$1] export $func$2 as \"test\" " +
"function $func$0($var$0:i32,$var$1:f32) : () { nop } " +
"function $func$1($var$0:i32) : (i32) { $var$0 } " +
"function $func$2() : () {" +
" call $func$0 (call_indirect $type$0 [1] (2),call_import $import$0 (1.0f)) " +
"} memory 1,65535 {} ");
