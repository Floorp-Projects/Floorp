// |jit-test| skip-if: !wasmTailCallsEnabled()

// Basically try to assert that the instance is restored properly when
// performing an import call.  We do this by accessing the instance once we get
// out of the tail call chain.

// In this pure form, this test can't test for stack overflow as we can't do a loop of
// imported functions without return_call_indirect and a table.

// TODO: Probably add a loop here with an indirect call
// TODO: More ballast will make it more likely that stack smashing is detected

var insh = wasmEvalText(`
(module
  (global $glob (export "glob") (mut i32) (i32.const 12345678))
  (func $h (export "h") (param i32) (result i32)
    (local.get 0)))`);

var insg = wasmEvalText(`
(module
  (import "insh" "h" (func $h (param i32) (result i32)))
  (global $glob (export "glob") (mut i32) (i32.const 24680246))
  (func $g (export "g") (param i32) (result i32)
    (return_call $h (local.get 0))))`, {insh:insh.exports});

var insf = wasmEvalText(`
(module
  (import "insg" "g" (func $g (param i32) (result i32)))
  (global $glob (export "glob") (mut i32) (i32.const 36903690))
  (func $f (export "f") (param i32) (result i32)
    (return_call $g (local.get 0))))`, {insg:insg.exports});

var start = wasmEvalText(`
(module
  (import "insf" "f" (func $f (param i32) (result i32)))
  (global $glob (export "glob") (mut i32) (i32.const 480480480))
  (func (export "run") (param i32) (result i32)
    (i32.add (call $f (local.get 0)) (global.get $glob))))`, {insf:insf.exports});

assertEq(start.exports.run(37), 480480480 + 37);

