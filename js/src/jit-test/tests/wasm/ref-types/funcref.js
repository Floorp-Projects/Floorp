const {Module,Instance,Global,RuntimeError} = WebAssembly;

const badWasmFunc = /can only pass WebAssembly exported functions to funcref/;
const typeErr = /type mismatch/;


// Validation:

wasmFailValidateText(`(module (func (local externref funcref) (local.set 0 (local.get 1))))`, typeErr);
wasmEvalText(`(module (func (local funcref funcref) (local.set 0 (local.get 1))))`);
wasmEvalText(`(module (func (local funcref) (local.set 0 (ref.null func))))`);
wasmFailValidateText(`(module (func (local funcref externref) (local.set 0 (local.get 1))))`, typeErr);
wasmEvalText(`(module (global (mut funcref) (ref.null func)) (func (param funcref) (global.set 0 (local.get 0))))`);
wasmFailValidateText(`(module (global (mut externref) (ref.null extern)) (func (param funcref) (global.set 0 (local.get 0))))`, typeErr);
wasmFailValidateText(`(module (global (mut funcref) (ref.null func)) (func (param externref) (global.set 0 (local.get 0))))`, typeErr);
wasmEvalText(`(module (func (param funcref)) (func (param funcref) (call 0 (local.get 0))))`);
wasmFailValidateText(`(module (func (param externref)) (func (param funcref) (call 0 (local.get 0))))`, typeErr);
wasmFailValidateText(`(module (func (param funcref)) (func (param externref) (call 0 (local.get 0))))`, typeErr);
wasmEvalText(`(module (func (param funcref) (result funcref) (block (result funcref) (local.get 0) (br 0))))`);
wasmFailValidateText(`(module (func (param funcref) (result externref) (block (result externref) (local.get 0) (br 0))))`, typeErr);
wasmFailValidateText(`(module (func (param externref) (result externref) (block (result funcref) (local.get 0) (br 0))))`, typeErr);
wasmEvalText(`(module (func (param funcref funcref) (result funcref) (select (result funcref) (local.get 0) (local.get 1) (i32.const 0))))`);
wasmFailValidateText(`(module (func (param externref funcref) (result externref) (select (result externref) (local.get 0) (local.get 1) (i32.const 0))))`, typeErr);
wasmFailValidateText(`(module (func (param funcref externref) (result externref) (select (result externref)(local.get 0) (local.get 1) (i32.const 0))))`, typeErr);
wasmFailValidateText(`(module (func (param externref funcref) (result funcref) (select (result funcref) (local.get 0) (local.get 1) (i32.const 0))))`, typeErr);
wasmFailValidateText(`(module (func (param funcref externref) (result funcref) (select (result funcref) (local.get 0) (local.get 1) (i32.const 0))))`, typeErr);


// Runtime:

var m = new Module(wasmTextToBinary(`(module (func (export "wasmFun")))`));
const wasmFun1 = new Instance(m).exports.wasmFun;
const wasmFun2 = new Instance(m).exports.wasmFun;
const wasmFun3 = new Instance(m).exports.wasmFun;

var run = wasmEvalText(`(module
    (global (mut funcref) (ref.null func))
    (func (param $x funcref) (param $test i32) (result funcref)
      local.get $x
      global.get 0
      local.get $test
      select (result funcref)
    )
    (func (export "run") (param $a funcref) (param $b funcref) (param $c funcref) (param $test1 i32) (param $test2 i32) (result funcref)
      local.get $a
      global.set 0
      block (result funcref)
        local.get $b
        local.get $test1
        br_if 0
        drop
        local.get $c
      end
      local.get $test2
      call 0
    )
)`).exports.run;
assertEq(run(wasmFun1, wasmFun2, wasmFun3, false, false), wasmFun1);
assertEq(run(wasmFun1, wasmFun2, wasmFun3, true, false), wasmFun1);
assertEq(run(wasmFun1, wasmFun2, wasmFun3, true, true), wasmFun2);
assertEq(run(wasmFun1, wasmFun2, wasmFun3, false, true), wasmFun3);

var run = wasmEvalText(`(module
  (type $t0 (func (param externref) (result externref)))
  (type $t1 (func (param funcref) (result externref)))
  (type $t2 (func (param funcref funcref) (result externref)))
  (func $f0 (type $t0) ref.null extern)
  (func $f1 (type $t1) ref.null extern)
  (func $f2 (type $t2) ref.null extern)
  (table funcref (elem $f0 $f1 $f2))
  (func (export "run") (param i32 i32) (result externref)
    block $b2 block $b1 block $b0
      local.get 0
      br_table $b0 $b1 $b2
    end $b0
      ref.null extern
      local.get 1
      call_indirect (type $t0)
      return
    end $b1
      ref.null func
      local.get 1
      call_indirect (type $t1)
      return
    end $b2
      ref.null func
      ref.null func
      local.get 1
      call_indirect (type $t2)
      return
  )
)`).exports.run;

for (var i = 0; i < 3; i++) {
  for (var j = 0; j < 3; j++) {
    if (i == j)
      assertEq(run(i, j), null);
    else
      assertErrorMessage(() => run(i, j), RuntimeError, /indirect call signature mismatch/);
  }
}


// JS API:

const wasmFun = wasmEvalText(`(module (func (export "x")))`).exports.x;

var run = wasmEvalText(`(module (func (export "run") (param funcref) (result funcref) (local.get 0)))`).exports.run;
assertEq(run(wasmFun), wasmFun);
assertEq(run(null), null);
assertErrorMessage(() => run(() => {}), TypeError, badWasmFunc);

var importReturnValue;
var importFun = () => importReturnValue;
var run = wasmEvalText(`(module (func (import "" "i") (result funcref)) (func (export "run") (result funcref) (call 0)))`, {'':{i:importFun}}).exports.run;
importReturnValue = wasmFun;
assertEq(run(), wasmFun);
importReturnValue = null;
assertEq(run(), null);
importReturnValue = undefined;
assertErrorMessage(() => run(), TypeError, badWasmFunc);
importReturnValue = () => {};
assertErrorMessage(() => run(), TypeError, badWasmFunc);

var g = new Global({value:'funcref', mutable:true}, wasmFun);
assertEq(g.value, wasmFun);
g.value = null;
assertEq(g.value, null);
Math.sin();
assertErrorMessage(() => g.value = () => {}, TypeError, badWasmFunc);
var g = new Global({value:'funcref', mutable:true}, null);
assertEq(g.value, null);
g.value = wasmFun;
assertEq(g.value, wasmFun);
assertErrorMessage(() => new Global({value:'funcref'}, () => {}), TypeError, badWasmFunc);
