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

if (!wasmGcEnabled())
    quit(0);

// Ion can't talk about references yet *but* we can call indirect from Ion to a
// baseline module that has exported a function that accepts or returns anyref,
// without the caller knowing this or having to declare it.  All such calls
// should fail in an orderly manner with a type mismatch, at the point of the
// call.

var refmod = new WebAssembly.Module(wasmTextToBinary(
    `(module
      (gc_feature_opt_in 1)

      (import $tbl "" "tbl" (table 4 anyfunc))
      (import $print "" "print" (func (param i32)))

      (type $htype (func (param anyref)))
      (type $itype (func (result anyref)))

      (elem (i32.const 0) $f $g)

      (func $f (param anyref)
       (call $print (i32.const 1)))

      (func $g (result anyref)
       (call $print (i32.const 2))
       (ref.null anyref))

      (func (export "test_h")
       (call_indirect $htype (ref.null anyref) (i32.const 2)))

      (func (export "test_i")
       (drop (call_indirect $itype (i32.const 3))))

     )`));

var nonrefmod = new WebAssembly.Module(wasmTextToBinary(
    `(module
      (import $tbl "" "tbl" (table 4 anyfunc))
      (import $print "" "print" (func (param i32)))

      (type $ftype (func (param i32)))
      (type $gtype (func (result i32)))

      (elem (i32.const 2) $h $i)

      ;; Should fail because of the signature mismatch: parameter
      (func (export "test_f")
       (call_indirect $ftype (i32.const 37) (i32.const 0)))

      ;; Should fail because of the signature mismatch: return value
      (func (export "test_g")
       (drop (call_indirect $gtype (i32.const 1))))

      (func $h (param i32)
       (call $print (i32.const 2)))

      (func $i (result i32)
       (call $print (i32.const 3))
       (i32.const 37))
     )`));

var tbl = new WebAssembly.Table({initial:4, element:"anyfunc"});
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

