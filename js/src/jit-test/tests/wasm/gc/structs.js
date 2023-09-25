// |jit-test| skip-if: !wasmGcEnabled()

// This tests a bunch of wasm struct stuff, but not i8 or i16 fields.
// See structs2.js for i8/i16 field tests.

var bin = wasmTextToBinary(
    `(module
      (func $x1 (import "m" "x1") (type $f1))
      (func $x2 (import "m" "x2") (type $f2))

      (table 2 funcref)
      (elem (i32.const 0) $doit $doitagain)

      ;; Type array has a mix of types

      (type $f1 (func (param i32) (result i32)))

      (type $point (struct
                    (field $point_x i32)
                    (field $point_y i32)))

      (type $f2 (func (param f64) (result f64)))

      (type $int_node (struct
                       (field $intbox_val (mut i32))
                       (field $intbox_next (mut externref))))

      ;; Test all the types.

      (type $omni (struct
                   (field $omni_i32 i32)
                   (field $omni_i32m (mut i32))
                   (field $omni_i64 i64)
                   (field $omni_i64m (mut i64))
                   (field $omni_f32 f32)
                   (field $omni_f32m (mut f32))
                   (field $omni_f64 f64)
                   (field $omni_f64m (mut f64))
                   (field $omni_externref externref)
                   (field $omni_externrefm (mut externref))))

      ;; Various ways to reference a type in the middle of the
      ;; type array, make sure we get the right one

      (func (export "hello") (param f64) (param i32) (result f64)
       (call_indirect (type $f2) (local.get 0) (local.get 1)))

      (func $doit (param f64) (result f64)
       (f64.sqrt (local.get 0)))

      (func $doitagain (param f64) (result f64)
       (f64.mul (local.get 0) (local.get 0)))

      (func (export "x1") (param i32) (result i32)
       (call $x1 (local.get 0)))

      (func (export "x2") (param f64) (result f64)
       (call $x2 (local.get 0)))

      ;; Useful for testing to ensure that the type is not type #0 here.

      (func (export "mk_point") (result eqref)
       (struct.new $point (i32.const 37) (i32.const 42)))

      (func (export "mk_int_node") (param i32) (param externref) (result eqref)
       (struct.new $int_node (local.get 0) (local.get 1)))

      ;; Too big to fit in an inline WasmGcObject.

      (type $bigger (struct
                     (field $a i32)
                     (field $b i32)
                     (field $c i32)
                     (field $d i32)
                     (field $e i32)
                     (field $f i32)
                     (field $g i32)
                     (field $h i32)
                     (field $i i32)
                     (field $j i32)
                     (field $k i32)
                     (field $l i32)
                     (field $m i32)
                     (field $n i32)
                     (field $o i32)
                     (field $p i32)
                     (field $q i32)
                     (field $r i32)
                     (field $s i32)
                     (field $t i32)
                     (field $u i32)
                     (field $v i32)
                     (field $w i32)
                     (field $x i32)
                     (field $y i32)
                     (field $z i32)
                     (field $aa i32)
                     (field $ab i32)
                     (field $ac i32)
                     (field $ad i32)
                     (field $ae i32)
                     (field $af i32)
                     (field $ag i32)
                     (field $ah i32)
                     (field $ai i32)
                     (field $aj i32)
                     (field $ak i32)
                     (field $al i32)
                     (field $am i32)
                     (field $an i32)
                     (field $ao i32)
                     (field $ap i32)
                     (field $aq i32)
                     (field $ar i32)
                     (field $as i32)
                     (field $at i32)
                     (field $au i32)
                     (field $av i32)
                     (field $aw i32)
                     (field $ax i32)
                     (field $ay i32)
                     (field $az i32)))

      (func (export "mk_bigger") (result eqref)
            (struct.new $bigger
                       (i32.const 0)
                       (i32.const 1)
                       (i32.const 2)
                       (i32.const 3)
                       (i32.const 4)
                       (i32.const 5)
                       (i32.const 6)
                       (i32.const 7)
                       (i32.const 8)
                       (i32.const 9)
                       (i32.const 10)
                       (i32.const 11)
                       (i32.const 12)
                       (i32.const 13)
                       (i32.const 14)
                       (i32.const 15)
                       (i32.const 16)
                       (i32.const 17)
                       (i32.const 18)
                       (i32.const 19)
                       (i32.const 20)
                       (i32.const 21)
                       (i32.const 22)
                       (i32.const 23)
                       (i32.const 24)
                       (i32.const 25)
                       (i32.const 26)
                       (i32.const 27)
                       (i32.const 28)
                       (i32.const 29)
                       (i32.const 30)
                       (i32.const 31)
                       (i32.const 32)
                       (i32.const 33)
                       (i32.const 34)
                       (i32.const 35)
                       (i32.const 36)
                       (i32.const 37)
                       (i32.const 38)
                       (i32.const 39)
                       (i32.const 40)
                       (i32.const 41)
                       (i32.const 42)
                       (i32.const 43)
                       (i32.const 44)
                       (i32.const 45)
                       (i32.const 46)
                       (i32.const 47)
                       (i32.const 48)
                       (i32.const 49)
                       (i32.const 50)
                       (i32.const 51)))

      (type $withfloats (struct
                         (field $f1 f32)
                         (field $f2 f64)
                         (field $f3 externref)
                         (field $f4 f32)
                         (field $f5 i32)))

      (func (export "mk_withfloats")
            (param f32) (param f64) (param externref) (param f32) (param i32)
            (result eqref)
            (struct.new $withfloats (local.get 0) (local.get 1) (local.get 2) (local.get 3) (local.get 4)))

     )`)

var mod = new WebAssembly.Module(bin);
var ins = new WebAssembly.Instance(mod, {m:{x1(x){ return x*3 }, x2(x){ return Math.PI }}}).exports;

assertEq(ins.hello(4.0, 0), 2.0)
assertEq(ins.hello(4.0, 1), 16.0)

assertEq(ins.x1(12), 36)
assertEq(ins.x2(8), Math.PI)

var point = ins.mk_point();
assertEq(wasmGcReadField(point, 0), 37);
assertEq(wasmGcReadField(point, 1), 42);

var int_node = ins.mk_int_node(78, point);
assertEq(wasmGcReadField(int_node, 0), 78);
assertEq(wasmGcReadField(int_node, 1), point);

var bigger = ins.mk_bigger();
for ( let i=0; i < 52; i++ )
    assertEq(wasmGcReadField(bigger, i), i);

var withfloats = ins.mk_withfloats(1/3, Math.PI, bigger, 5/6, 0x1337);
assertEq(wasmGcReadField(withfloats, 0), Math.fround(1/3));
assertEq(wasmGcReadField(withfloats, 1), Math.PI);
assertEq(wasmGcReadField(withfloats, 2), bigger);
assertEq(wasmGcReadField(withfloats, 3), Math.fround(5/6));
assertEq(wasmGcReadField(withfloats, 4), 0x1337);

// A simple stress test

var stress = wasmTextToBinary(
    `(module
      (type $node (struct (field i32) (field (ref null $node))))
      (func (export "iota1") (param $n i32) (result eqref)
       (local $list (ref null $node))
       (block $exit
        (loop $loop
         (br_if $exit (i32.eqz (local.get $n)))
         (local.set $list (struct.new $node (local.get $n) (local.get $list)))
         (local.set $n (i32.sub (local.get $n) (i32.const 1)))
         (br $loop)))
       (local.get $list)))`);
var stressIns = new WebAssembly.Instance(new WebAssembly.Module(stress)).exports;
var stressLevel =
    getBuildConfiguration("x64") && !getBuildConfiguration("tsan") &&
            !getBuildConfiguration("asan") && !getBuildConfiguration("valgrind")
        ? 100000
        : 1000;
var the_list = stressIns.iota1(stressLevel);
for (let i=1; i <= stressLevel; i++) {
    assertEq(wasmGcReadField(the_list, 0), i);
    the_list = wasmGcReadField(the_list, 1);
}
assertEq(the_list, null);

// Fields and their exposure in JS.  We can't export types yet so hide them
// inside the module with globals.

// i64 fields.

{
    let txt =
        `(module
          (type $big (struct
                      (field (mut i32))
                      (field (mut i64))
                      (field (mut i32))))

          (func (export "set") (param eqref)
           (local (ref null $big))
           (local.set 1 (ref.cast (ref null $big) (local.get 0)))
           (struct.set $big 1 (local.get 1) (i64.const 0x3333333376544567)))

          (func (export "set2") (param $p eqref)
           (struct.set $big 1
            (ref.cast (ref null $big) (local.get $p))
            (i64.const 0x3141592653589793)))

          (func (export "low") (param $p eqref) (result i32)
           (i32.wrap_i64 (struct.get $big 1 (ref.cast (ref null $big) (local.get $p)))))

          (func (export "high") (param $p eqref) (result i32)
           (i32.wrap_i64 (i64.shr_u
                          (struct.get $big 1 (ref.cast (ref null $big) (local.get $p)))
                          (i64.const 32))))

          (func (export "mk") (result eqref)
           (struct.new $big (i32.const 0x7aaaaaaa) (i64.const 0x4201020337) (i32.const 0x6bbbbbbb)))

         )`;

    let ins = wasmEvalText(txt).exports;

    let v = ins.mk();
    assertEq(typeof v, "object");
    assertEq(wasmGcReadField(v, 0), 0x7aaaaaaa);
    assertEq(wasmGcReadField(v, 1), 0x4201020337n);
    assertEq(ins.low(v), 0x01020337);
    assertEq(ins.high(v), 0x42);
    assertEq(wasmGcReadField(v, 2), 0x6bbbbbbb);

    ins.set(v);
    assertEq(wasmGcReadField(v, 0), 0x7aaaaaaa);
    assertEq(wasmGcReadField(v, 1), 0x3333333376544567n);
    assertEq(wasmGcReadField(v, 2), 0x6bbbbbbb);

    ins.set2(v);
    assertEq(wasmGcReadField(v, 1), 0x3141592653589793n);
    assertEq(ins.low(v), 0x53589793);
    assertEq(ins.high(v), 0x31415926)
}

{
    let txt =
        `(module
          (type $big (struct
                      (field (mut i32))
                      (field (mut i64))
                      (field (mut i32))))

          (global $g (mut (ref null $big)) (ref.null $big))

          (func (export "make") (result eqref)
           (global.set $g
            (struct.new $big (i32.const 0x7aaaaaaa) (i64.const 0x4201020337) (i32.const 0x6bbbbbbb)))
           (global.get $g))

          (func (export "update0") (param $x i32)
           (struct.set $big 0 (global.get $g) (local.get $x)))

          (func (export "get0") (result i32)
           (struct.get $big 0 (global.get $g)))

          (func (export "update1") (param $hi i32) (param $lo i32)
           (struct.set $big 1 (global.get $g)
            (i64.or
             (i64.shl (i64.extend_i32_u (local.get $hi)) (i64.const 32))
             (i64.extend_i32_u (local.get $lo)))))

          (func (export "get1_low") (result i32)
           (i32.wrap_i64 (struct.get $big 1 (global.get $g))))

          (func (export "get1_high") (result i32)
           (i32.wrap_i64
            (i64.shr_u (struct.get $big 1 (global.get $g)) (i64.const 32))))

          (func (export "update2") (param $x i32)
           (struct.set $big 2 (global.get $g) (local.get $x)))

          (func (export "get2") (result i32)
           (struct.get $big 2 (global.get $g)))

         )`;

    let ins = wasmEvalText(txt).exports;

    let v = ins.make();
    assertEq(wasmGcReadField(v, 0), 0x7aaaaaaa);
    assertEq(wasmGcReadField(v, 1), 0x4201020337n);
    assertEq(wasmGcReadField(v, 2), 0x6bbbbbbb);

    ins.update0(0x45367101);
    assertEq(wasmGcReadField(v, 0), 0x45367101);
    assertEq(ins.get0(), 0x45367101);
    assertEq(wasmGcReadField(v, 1), 0x4201020337n);
    assertEq(wasmGcReadField(v, 2), 0x6bbbbbbb);

    ins.update2(0x62345123);
    assertEq(wasmGcReadField(v, 0), 0x45367101);
    assertEq(wasmGcReadField(v, 1), 0x4201020337n);
    assertEq(ins.get2(), 0x62345123);
    assertEq(wasmGcReadField(v, 2), 0x62345123);

    ins.update1(0x77777777, 0x22222222);
    assertEq(wasmGcReadField(v, 0), 0x45367101);
    assertEq(ins.get1_low(), 0x22222222);
    assertEq(ins.get1_high(), 0x77777777);
    assertEq(wasmGcReadField(v, 1), 0x7777777722222222n);
    assertEq(wasmGcReadField(v, 2), 0x62345123);
}


var bin = wasmTextToBinary(
    `(module
      (type $cons (struct (field i32) (field (ref null $cons))))

      (global $g (mut (ref null $cons)) (ref.null $cons))

      (func (export "push") (param i32)
       (global.set $g (struct.new $cons (local.get 0) (global.get $g))))

      (func (export "top") (result i32)
       (struct.get $cons 0 (global.get $g)))

      (func (export "pop")
       (global.set $g (struct.get $cons 1 (global.get $g))))

      (func (export "is_empty") (result i32)
       (ref.is_null (global.get $g)))

      )`);

var mod = new WebAssembly.Module(bin);
var ins = new WebAssembly.Instance(mod).exports;
ins.push(37);
ins.push(42);
ins.push(86);
assertEq(ins.top(), 86);
ins.pop();
assertEq(ins.top(), 42);
ins.pop();
assertEq(ins.top(), 37);
ins.pop();
assertEq(ins.is_empty(), 1);
assertErrorMessage(() => ins.pop(),
                   WebAssembly.RuntimeError,
                   /dereferencing null pointer/);

// Check that a wrapped object cannot be passed as an eqref even if the wrapper
// points to the right type.  This is a temporary restriction, until we're able
// to avoid dealing with wrappers inside the engine.

{
    var ins = wasmEvalText(
        `(module
          (type $Node (struct (field i32)))
          (func (export "mk") (result eqref)
           (struct.new $Node (i32.const 37)))
          (func (export "f") (param $n eqref) (result eqref)
           (ref.cast (ref null $Node) (local.get $n))))`).exports;
    var n = ins.mk();
    assertEq(ins.f(n), n);
    assertErrorMessage(() => ins.f(wrapWithProto(n, {})), TypeError, /can only pass a WebAssembly GC object/);
}

// Field names.

// Test that names map to the right fields.

{
    let ins = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(
        `(module
          (type $s (struct
                    (field $x i32)
                    (field $y i32)))

          (func $f (param $p (ref null $s)) (result i32)
           (struct.get $s $x (local.get $p)))

          (func $g (param $p (ref null $s)) (result i32)
           (struct.get $s $y (local.get $p)))

          (func (export "testf") (param $n i32) (result i32)
           (call $f (struct.new $s (local.get $n) (i32.mul (local.get $n) (i32.const 2)))))

          (func (export "testg") (param $n i32) (result i32)
           (call $g (struct.new $s (local.get $n) (i32.mul (local.get $n) (i32.const 2)))))

         )`))).exports;

    assertEq(ins.testf(10), 10);
    assertEq(ins.testg(10), 20);
}

// negative tests

// Wrong type passed as initializer

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(`
(module
  (type $r (struct (field i32)))
  (func $f (param f64) (result eqref)
    (struct.new $r (local.get 0)))
)`)),
WebAssembly.CompileError, /type mismatch/);

// Too few values passed for initializer

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(`
(module
  (type $r (struct (field i32) (field i32)))
  (func $f (result eqref)
    (struct.new $r (i32.const 0)))
)`)),
WebAssembly.CompileError, /popping value from empty stack/);

// Too many values passed for initializer, sort of

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(`
(module
  (type $r (struct (field i32) (field i32)))
  (func $f (result eqref)
    (i32.const 0)
    (i32.const 1)
    (i32.const 2)
    struct.new $r)
)`)),
WebAssembly.CompileError, /unused values/);

// Not referencing a structure type

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(`
(module
  (type (func (param i32) (result i32)))
  (func $f (result eqref)
    (struct.new 0))
)`)),
WebAssembly.CompileError, /not a struct type/);

// Nominal type equivalence for structs, but the prefix rule allows this
// conversion to succeed.

wasmEvalText(`
 (module
   (type $p (struct (field i32)))
   (type $q (struct (field i32)))
   (func $f (result (ref null $p))
    (struct.new $q (i32.const 0))))
`);

// The field name is optional, so this should work.

wasmEvalText(`
(module
 (type $s (struct (field i32))))
`)

// Empty structs are OK.

wasmEvalText(`
(module
 (type $s (struct)))
`)

// Multiply defined structures.

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct (field $x i32)))
 (type $s (struct (field $y i32))))
`),
SyntaxError, /duplicate type identifier/);

// Bogus type definition syntax.

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s))
`),
SyntaxError, /wasm text error/);

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (field $x i32)))
`),
SyntaxError, /expected one of: `func`, `struct`, `array`/);

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct (field $x i31))))
`),
SyntaxError, /wasm text error/);

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct (fjeld $x i32))))
`),
SyntaxError, /wasm text error/);

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct abracadabra)))
`),
SyntaxError, /wasm text error/);

// Function should not reference struct type: syntactic test

assertErrorMessage(() => wasmEvalText(`
(module
 (type $s (struct))
 (type $f (func (param i32) (result i32)))
 (func (type 0) (param i32) (result i32) (unreachable)))
`),
WebAssembly.CompileError, /signature index references non-signature/);

// Can't set immutable fields from JS

{
    let ins = wasmEvalText(
        `(module
          (type $s (struct
                    (field i32)
                    (field (mut i64))))
          (func (export "make") (result eqref)
           (struct.new $s (i32.const 37) (i64.const 42))))`).exports;
    let v = ins.make();
    assertErrorMessage(() => v[0] = 12, TypeError, /can't modify/);
    assertErrorMessage(() => v[1] = 12, TypeError, /can't modify/);
}

// Function should not reference struct type: binary test

var bad = new Uint8Array([0x00, 0x61, 0x73, 0x6d,
                          0x01, 0x00, 0x00, 0x00,

                          0x01,                   // Type section
                          0x03,                   // Section size
                          0x01,                   // One type
                          0x5f,                   // Struct
                          0x00,                   // Zero fields

                          0x03,                   // Function section
                          0x02,                   // Section size
                          0x01,                   // One function
                          0x00,                   // Type of function

                          0x0a,                   // Code section
                          0x05,                   // Section size
                          0x01,                   // One body
                          0x03,                   // Body size
                          0x00,                   // Zero locals
                          0x00,                   // UNREACHABLE
                          0x0b]);                 // END

assertErrorMessage(() => new WebAssembly.Module(bad),
                   WebAssembly.CompileError, /signature index references non-signature/);

// Exercise alias-analysis code for struct access
{
    let txt =
    `(module
       (type $meh (struct))
       (type $hasOOL (struct
                      ;; In-line storage
                      (field i64) (field i64)
                      (field $ILnonref (mut i64)) (field $ILref (mut eqref))
                      (field i64) (field i64) (field i64) (field i64)
                      (field i64) (field i64) (field i64) (field i64)
                      (field i64) (field i64) (field i64) (field i64)
                      ;; Out-of-line storage (or maybe it starts earlier, but
                      ;; definitely not after this point).
                      (field $OOLnonref (mut i64)) (field $OOLref (mut eqref)))
       )
       (func (export "create") (result eqref)
         (struct.new $hasOOL
           (i64.const 1)    (i64.const 2)
           (i64.const 9876) (ref.null $meh)
           (i64.const 3)    (i64.const 4)   (i64.const 5)   (i64.const 6)
           (i64.const 7)    (i64.const 8)   (i64.const 9)   (i64.const 10)
           (i64.const 11)   (i64.const 12)  (i64.const 13)  (i64.const 14)
           (i64.const 4321) (ref.null $meh))
       )
       ;; Write to an OOL field, then an IL field, then to an OOL field, so
       ;; that we can at least check (from inspection of the optimised MIR)
       ;; that the GVN+alias analysis causes the OOL block pointer not to be
       ;; reloaded for the second OOL write.  First for non-ref fields ..
       (func (export "threeSetsNonReffy") (param eqref)
         (local (ref $hasOOL))
         (local.set 1 (ref.as_non_null (ref.cast (ref null $hasOOL) (local.get 0))))
         (struct.set $hasOOL 16 (local.get 1) (i64.const 1337)) ;; set $OOLnonref
         (struct.set $hasOOL 2  (local.get 1) (i64.const 7331)) ;; set $ILnonref
         (struct.set $hasOOL 16 (local.get 1) (i64.const 9009)) ;; set $OOLnonref
       )
       ;; and the same for ref fields.
       (func (export "threeSetsReffy") (param eqref)
         (local (ref $hasOOL))
         (local.set 1 (ref.as_non_null (ref.cast (ref null $hasOOL) (local.get 0))))
         (struct.set $hasOOL 17 (local.get 1) (ref.null $meh)) ;; set $OOLref
         (struct.set $hasOOL 3  (local.get 1) (ref.null $meh)) ;; set $ILref
         (struct.set $hasOOL 17 (local.get 1) (ref.null $meh)) ;; set $OOLref
       )
     )`;
    let exports = wasmEvalText(txt).exports;
}

//////////////////////////////////////////////////////////////////////////////
//
// Checks for requests to create structs with more than MaxStructFields, where
// MaxStructFields == 1000.

function structNewOfManyFields(numFields) {
    let defString = "(type $s (struct ";
    for (i = 0; i < numFields; i++) {
        defString += "(field i32) ";
    }
    defString += "))";

    let insnString = "(struct.new $s ";
    for (i = 0; i < numFields; i++) {
        insnString += "(i32.const 1337) ";
    }
    insnString += ")";

    return "(module " +
           defString +
           " (func (export \"create\") (result eqref) " +
           insnString +
           "))";
}

{
    // 10_000 fields is allowable
    let exports = wasmEvalText(structNewOfManyFields(10000)).exports;
    let s = exports.create();
    assertEq(s, s);
}
{
    // but 10_001 is not
    assertErrorMessage(() => wasmEvalText(structNewOfManyFields(10001)),
                       WebAssembly.CompileError,
                       /too many fields in struct/);
}

// FIXME: also check struct.new_default, once it is available in both compilers.
