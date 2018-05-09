if (!wasmGcEnabled())
    quit();

var conf = getBuildConfiguration();

var bin = wasmTextToBinary(
    `(module
      (gc_feature_opt_in 1)

      (table 2 anyfunc)
      (elem (i32.const 0) $doit $doitagain)

      ;; Type array has a mix of types

      (type $f1 (func (param i32) (result i32)))

      (type $point (struct
                    (field $point_x i32)
                    (field $point_y i32)))

      (type $f2 (func (param f64) (result f64)))

      (type $int_node (struct
                       (field $intbox_val (mut i32))
                       (field $intbox_next (mut anyref))))

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
                   (field $omni_anyref anyref)
                   (field $omni_anyrefm (mut anyref))))

      ;; Various ways to reference a type in the middle of the
      ;; type array, make sure we get the right one

      (func $x1 (import "m" "x1") (type $f1))
      (func $x2 (import "m" "x2") (type $f2))

      (func (export "hello") (param f64) (param i32) (result f64)
       (call_indirect $f2 (get_local 0) (get_local 1)))

      (func $doit (param f64) (result f64)
       (f64.sqrt (get_local 0)))

      (func $doitagain (param f64) (result f64)
       (f64.mul (get_local 0) (get_local 0)))

      (func (export "x1") (param i32) (result i32)
       (call $x1 (get_local 0)))

      (func (export "x2") (param f64) (result f64)
       (call $x2 (get_local 0)))

      ;; Useful for testing to ensure that the type is not type #0 here.

      (func (export "mk_point") (result anyref)
       (struct.new $point (i32.const 37) (i32.const 42)))

      (func (export "mk_int_node") (param i32) (param anyref) (result anyref)
       (struct.new $int_node (get_local 0) (get_local 1)))

      ;; Too big to fit in an InlineTypedObject.

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

      (func (export "mk_bigger") (result anyref)
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
                         (field $f3 anyref)
                         (field $f4 f32)
                         (field $f5 i32)))

      (func (export "mk_withfloats")
            (param f32) (param f64) (param anyref) (param f32) (param i32)
            (result anyref)
            (struct.new $withfloats (get_local 0) (get_local 1) (get_local 2) (get_local 3) (get_local 4)))

     )`)

var mod = new WebAssembly.Module(bin);
var ins = new WebAssembly.Instance(mod, {m:{x1(x){ return x*3 }, x2(x){ return Math.PI }}}).exports;

assertEq(ins.hello(4.0, 0), 2.0)
assertEq(ins.hello(4.0, 1), 16.0)

assertEq(ins.x1(12), 36)
assertEq(ins.x2(8), Math.PI)

var point = ins.mk_point();
assertEq("_0" in point, true);
assertEq("_1" in point, true);
assertEq("_2" in point, false);
assertEq(point._0, 37);
assertEq(point._1, 42);

var int_node = ins.mk_int_node(78, point);
assertEq(int_node._0, 78);
assertEq(int_node._1, point);

var bigger = ins.mk_bigger();
for ( let i=0; i < 52; i++ )
    assertEq(bigger["_" + i], i);

var withfloats = ins.mk_withfloats(1/3, Math.PI, bigger, 5/6, 0x1337);
assertEq(withfloats._0, Math.fround(1/3));
assertEq(withfloats._1, Math.PI);
assertEq(withfloats._2, bigger);
assertEq(withfloats._3, Math.fround(5/6));
assertEq(withfloats._4, 0x1337);

// A simple stress test

var stress = wasmTextToBinary(
    `(module
      (gc_feature_opt_in 1)
      (type $node (struct (field i32) (field (ref $node))))
      (func (export "iota1") (param $n i32) (result anyref)
       (local $list (ref $node))
       (block $exit
        (loop $loop
         (br_if $exit (i32.eqz (get_local $n)))
         (set_local $list (struct.new $node (get_local $n) (get_local $list)))
         (set_local $n (i32.sub (get_local $n) (i32.const 1)))
         (br $loop)))
       (get_local $list)))`);
var stressIns = new WebAssembly.Instance(new WebAssembly.Module(stress)).exports;
var stressLevel = conf.x64 && !conf.tsan && !conf.asan && !conf.valgrind ? 100000 : 1000;
var the_list = stressIns.iota1(stressLevel);
for (let i=1; i <= stressLevel; i++) {
    assertEq(the_list._0, i);
    the_list = the_list._1;
}
assertEq(the_list, null);

// i64 fields.

{
    let txt =
        `(module
          (gc_feature_opt_in 1)

          (type $big (struct
                      (field (mut i32))
                      (field (mut i64))
                      (field (mut i32))))

          (func (export "mk") (result anyref)
           (struct.new $big (i32.const 0x7aaaaaaa) (i64.const 0x4201020337) (i32.const 0x6bbbbbbb)))

         )`;

    let ins = wasmEvalText(txt).exports;

    let v = ins.mk();
    assertEq(typeof v, "object");
    assertEq(v._0, 0x7aaaaaaa);
    assertEq(v._1_low, 0x01020337);
    assertEq(v._1_high, 0x42);

    v._0 = 0x5ccccccc;
    v._2 = 0x4ddddddd;
    assertEq(v._1_low, 0x01020337);
}

// negative tests

// Wrong type passed as initializer

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(`
(module
  (gc_feature_opt_in 1)
  (type $r (struct (field i32)))
  (func $f (param f64) (result anyref)
    (struct.new $r (get_local 0)))
)`)),
WebAssembly.CompileError, /type mismatch/);

// Too few values passed for initializer

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(`
(module
  (gc_feature_opt_in 1)
  (type $r (struct (field i32) (field i32)))
  (func $f (result anyref)
    (struct.new $r (i32.const 0)))
)`)),
WebAssembly.CompileError, /popping value from empty stack/);

// Too many values passed for initializer, sort of

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(`
(module
  (gc_feature_opt_in 1)
  (type $r (struct (field i32) (field i32)))
  (func $f (result anyref)
    (i32.const 0)
    (i32.const 1)
    (i32.const 2)
    struct.new $r)
)`)),
WebAssembly.CompileError, /unused values/);

// Not referencing a structure type

assertErrorMessage(() => new WebAssembly.Module(wasmTextToBinary(`
(module
  (gc_feature_opt_in 1)
  (type (func (param i32) (result i32)))
  (func $f (result anyref)
    (struct.new 0))
)`)),
WebAssembly.CompileError, /not a struct type/);

// Nominal type equivalence for structs, but the prefix rule allows this
// conversion to succeed.

wasmEvalText(`
 (module
   (gc_feature_opt_in 1)
   (type $p (struct (field i32)))
   (type $q (struct (field i32)))
   (func $f (result (ref $p))
    (struct.new $q (i32.const 0))))
`);

// The field name is optional, so this should work.

wasmEvalText(`
(module
 (gc_feature_opt_in 1)
 (type $s (struct (field i32))))
`)

// Empty structs are OK.

wasmEvalText(`
(module
 (gc_feature_opt_in 1)
 (type $s (struct)))
`)

// Multiply defined structures.

assertErrorMessage(() => wasmEvalText(`
(module
 (gc_feature_opt_in 1)
 (type $s (struct (field $x i32)))
 (type $s (struct (field $y i32))))
`),
SyntaxError, /duplicate type name/);

// Bogus type definition syntax.

assertErrorMessage(() => wasmEvalText(`
(module
 (gc_feature_opt_in 1)
 (type $s))
`),
SyntaxError, /parsing wasm text/);

assertErrorMessage(() => wasmEvalText(`
(module
 (gc_feature_opt_in 1)
 (type $s (field $x i32)))
`),
SyntaxError, /bad type definition/);

assertErrorMessage(() => wasmEvalText(`
(module
 (gc_feature_opt_in 1)
 (type $s (struct (field $x i31))))
`),
SyntaxError, /parsing wasm text/);

assertErrorMessage(() => wasmEvalText(`
(module
 (gc_feature_opt_in 1)
 (type $s (struct (fjeld $x i32))))
`),
SyntaxError, /parsing wasm text/);

assertErrorMessage(() => wasmEvalText(`
(module
 (gc_feature_opt_in 1)
 (type $s (struct abracadabra)))
`),
SyntaxError, /parsing wasm text/);

// Function should not reference struct type: syntactic test

assertErrorMessage(() => wasmEvalText(`
(module
 (gc_feature_opt_in 1)
 (type $s (struct))
 (type $f (func (param i32) (result i32)))
 (func (type 0) (param i32) (result i32) (unreachable)))
`),
WebAssembly.CompileError, /signature index references non-signature/);

// Can't set immutable fields from JS

{
    let ins = wasmEvalText(
        `(module
          (gc_feature_opt_in 1)
          (type $s (struct
                    (field i32)
                    (field (mut i64))))
          (func (export "make") (result anyref)
           (struct.new $s (i32.const 37) (i64.const 42))))`).exports;
    let v = ins.make();
    assertErrorMessage(() => v._0 = 12,
                       Error,
                       /setting immutable field/);
    assertErrorMessage(() => v._1_low = 12,
                       Error,
                       /setting immutable field/);
    assertErrorMessage(() => v._1_high = 12,
                       Error,
                       /setting immutable field/);
}

// Function should not reference struct type: binary test

var bad = new Uint8Array([0x00, 0x61, 0x73, 0x6d,
                          0x01, 0x00, 0x00, 0x00,

                          0x2a,                   // GcFeatureOptIn section
                          0x01,                   // Section size
                          0x01,                   // Version

                          0x01,                   // Type section
                          0x03,                   // Section size
                          0x01,                   // One type
                          0x50,                   // Struct
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
