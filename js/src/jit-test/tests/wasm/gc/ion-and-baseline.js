// |jit-test| skip-if: !wasmGcEnabled()

// Attempt to test intercalls from ion to baseline and back.
//
// We get into this situation when the modules are compiled with different
// tiering policies, or one tiers up before the other, or (for now) one opts
// into gc and is baseline-compiled and the other does not and is ion-compiled.
// There are lots of variables here.  Generally, a small module will be
// ion-compiled unless there's reason to baseline-compile it, so we're likely
// actually testing something here.
//
// Some logging with printf confirms that refmod is baseline-compiled and
// nonrefmod is ion-compiled at present, with --wasm-gc enabled.

var refmod = new WebAssembly.Module(wasmTextToBinary(
    `(module
      (import "" "tbl" (table $tbl 4 funcref))
      (import "" "print" (func $print (param i32)))

      ;; Just a dummy
      (type $s (struct (field i32)))

      (type $htype (func (param anyref)))
      (type $itype (func (result anyref)))

      (elem (i32.const 0) $f $g)

      (func $f (param anyref)
       (call $print (i32.const 1)))

      (func $g (result anyref)
       (call $print (i32.const 2))
       (ref.null))

      (func (export "test_h")
       (call_indirect (type $htype) (ref.null) (i32.const 2)))

      (func (export "test_i")
       (drop (call_indirect (type $itype) (i32.const 3))))

     )`));

var nonrefmod = new WebAssembly.Module(wasmTextToBinary(
    `(module
      (import "" "tbl" (table $tbl 4 funcref))
      (import "" "print" (func $print (param i32)))

      (type $ftype (func (param i32)))
      (type $gtype (func (result i32)))

      (elem (i32.const 2) $h $i)

      ;; Should fail because of the signature mismatch: parameter
      (func (export "test_f")
       (call_indirect (type $ftype) (i32.const 37) (i32.const 0)))

      ;; Should fail because of the signature mismatch: return value
      (func (export "test_g")
       (drop (call_indirect (type $gtype) (i32.const 1))))

      (func $h (param i32)
       (call $print (i32.const 2)))

      (func $i (result i32)
       (call $print (i32.const 3))
       (i32.const 37))
     )`));

var tbl = new WebAssembly.Table({initial:4, element:"funcref"});
var refins = new WebAssembly.Instance(refmod, {"":{print, tbl}}).exports;
var nonrefins = new WebAssembly.Instance(nonrefmod, {"":{print, tbl}}).exports;

assertErrorMessage(() => nonrefins.test_f(),
                   WebAssembly.RuntimeError,
                   /indirect call signature mismatch/);

assertErrorMessage(() => nonrefins.test_g(),
                   WebAssembly.RuntimeError,
                   /indirect call signature mismatch/);

assertErrorMessage(() => refins.test_h(),
                   WebAssembly.RuntimeError,
                   /indirect call signature mismatch/);

assertErrorMessage(() => refins.test_i(),
                   WebAssembly.RuntimeError,
                   /indirect call signature mismatch/);

