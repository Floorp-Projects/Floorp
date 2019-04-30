// |jit-test| skip-if: !wasmReftypesEnabled()

const {Module,Instance,Global,RuntimeError} = WebAssembly;

const badWasmFunc = /can only pass WebAssembly exported functions to funcref/;
const typeErr = /type mismatch/;


// Validation:

wasmEvalText(`(module (func (local anyref funcref) (local.set 0 (local.get 1))))`);
wasmEvalText(`(module (func (local funcref funcref) (local.set 0 (local.get 1))))`);
wasmEvalText(`(module (func (local funcref) (local.set 0 (ref.null))))`);
wasmFailValidateText(`(module (func (local funcref anyref) (local.set 0 (local.get 1))))`, typeErr);
wasmEvalText(`(module (global (mut funcref) (ref.null)) (func (param funcref) (global.set 0 (local.get 0))))`);
wasmEvalText(`(module (global (mut anyref) (ref.null)) (func (param funcref) (global.set 0 (local.get 0))))`);
wasmFailValidateText(`(module (global (mut funcref) (ref.null)) (func (param anyref) (global.set 0 (local.get 0))))`, typeErr);
wasmEvalText(`(module (func (param funcref)) (func (param funcref) (call 0 (local.get 0))))`);
wasmEvalText(`(module (func (param anyref)) (func (param funcref) (call 0 (local.get 0))))`);
wasmFailValidateText(`(module (func (param funcref)) (func (param anyref) (call 0 (local.get 0))))`, typeErr);
wasmEvalText(`(module (func (param funcref) (result funcref) (block funcref (local.get 0) (br 0))))`);
wasmEvalText(`(module (func (param funcref) (result anyref) (block anyref (local.get 0) (br 0))))`);
wasmFailValidateText(`(module (func (param anyref) (result anyref) (block funcref (local.get 0) (br 0))))`, typeErr);
wasmEvalText(`(module (func (param funcref funcref) (result funcref) (select (local.get 0) (local.get 1) (i32.const 0))))`);
wasmEvalText(`(module (func (param anyref funcref) (result anyref) (select (local.get 0) (local.get 1) (i32.const 0))))`);
wasmEvalText(`(module (func (param funcref anyref) (result anyref) (select (local.get 0) (local.get 1) (i32.const 0))))`);
wasmFailValidateText(`(module (func (param anyref funcref) (result funcref) (select (local.get 0) (local.get 1) (i32.const 0))))`, typeErr);
wasmFailValidateText(`(module (func (param funcref anyref) (result funcref) (select (local.get 0) (local.get 1) (i32.const 0))))`, typeErr);


// Runtime:

var m = new Module(wasmTextToBinary(`(module (func (export "wasmFun")))`));
const wasmFun1 = new Instance(m).exports.wasmFun;
const wasmFun2 = new Instance(m).exports.wasmFun;
const wasmFun3 = new Instance(m).exports.wasmFun;

var run = wasmEvalText(`(module
    (global (mut funcref) (ref.null))
    (func (param $x funcref) (param $test i32) (result funcref)
      local.get $x
      global.get 0
      local.get $test
      select
    )
    (func (export "run") (param $a funcref) (param $b funcref) (param $c funcref) (param $test1 i32) (param $test2 i32) (result funcref)
      local.get $a
      global.set 0
      block funcref
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
  (type $t0 (func (param anyref) (result anyref)))
  (type $t1 (func (param funcref) (result anyref)))
  (type $t2 (func (param anyref) (result funcref)))
  (type $t3 (func (param funcref funcref) (result funcref)))
  (func $f0 (type $t0) ref.null)
  (func $f1 (type $t1) ref.null)
  (func $f2 (type $t2) ref.null)
  (func $f3 (type $t3) ref.null)
  (table funcref (elem $f0 $f1 $f2 $f3))
  (func (export "run") (param i32 i32) (result anyref)
    block $b3 block $b2 block $b1 block $b0
      local.get 0
      br_table $b0 $b1 $b2 $b3
    end $b0
      ref.null
      local.get 1
      call_indirect $t0
      return
    end $b1
      ref.null
      local.get 1
      call_indirect $t1
      return
    end $b2
      ref.null
      local.get 1
      call_indirect $t2
      return
    end $b3
      ref.null
      ref.null
      local.get 1
      call_indirect $t3
      return
  )
)`).exports.run;

for (var i = 0; i < 4; i++) {
  for (var j = 0; j < 4; j++) {
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
