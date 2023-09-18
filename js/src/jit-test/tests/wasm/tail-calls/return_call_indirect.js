var ins0 = wasmEvalText(`(module
  (func $fac-acc (export "fac-acc") (param i64 i64) (result i64)
    (if (result i64) (i64.eqz (local.get 0))
      (then (local.get 1))
      (else
        (return_call $fac-acc
          (i64.sub (local.get 0) (i64.const 1))
          (i64.mul (local.get 0) (local.get 1))
        )
      )
    )
  )
)`);

var ins = wasmEvalText(`(module
  (import "" "fac-acc" (func $fac-acc (param i64 i64) (result i64)))
  (type $ty (func (param i64 i64) (result i64)))
  (table $t 1 1 funcref)
  (func $f (export "fac") (param i64) (result i64)
    local.get 0
    i64.const 1
    i32.const 0
    return_call_indirect $t (type $ty)
  )
  (elem $t (i32.const 0) $fac-acc)

  (func (export "main") (result i64)
    i64.const 5
    call $f
  )
)`, {"": {"fac-acc": ins0.exports["fac-acc"]}});

// Check return call via wasm function
assertEq(ins.exports.main(5n), 120n);

// Check return call directly via interpreter stub
const fac = ins.exports["fac"];
assertEq(fac(4n, 1n), 24n);

// Check return call directly via jit stub
check_stub1: {
    let options = getJitCompilerOptions();
    if (!options["baseline.enable"]) break check_stub1;
    const check = function() {
        fac(4n, 1n);
    };
    for (let i = options["baseline.warmup.trigger"] + 1; i--;)
        check();
}

// Invalid func type
var ins = wasmEvalText(`(module
  (import "" "fac-acc" (func $fac-acc (param i64 i64) (result i64)))
  (type $ty (func (param i64 i64) (result i64)))
  (type $tz (func (param i64) (result i64)))
  (table $t 1 1 funcref)
  (func $f (export "fac") (param i64) (result i64)
    local.get 0
    i64.const 1
    i32.const 0
    return_call_indirect $t (type $tz)
  )
  (elem $t (i32.const 0) $fac-acc)

  (func (export "main") (result i64)
    i64.const 5
    call $f
  )
)`, {"": {"fac-acc": ins0.exports["fac-acc"]}});

assertErrorMessage(() => ins.exports.main(), WebAssembly.RuntimeError, /indirect call signature mismatch/);
assertErrorMessage(() => ins.exports.fac(6n), WebAssembly.RuntimeError, /indirect call signature mismatch/);

// Invalid func type, but testing when entry directly does invalid return_call_indirect.
var wasm = wasmTextToBinary(`(module
  (global $g (export "g") (mut i32) (i32.const 0))
  (table 1 1 funcref)
  (type $ft (func (param f64)))
  (func $f (export "f")
    unreachable
  )
  (func $test (export "test")
    global.get $g
    br_if 0
    f64.const 0.0
    i32.const 0
    return_call_indirect (type $ft)
  )
  (elem (i32.const 0) $f)
)`);

var ins = new WebAssembly.Instance(new WebAssembly.Module(wasm));
function check_err() { ins.exports.test(); }
assertErrorMessage(check_err, WebAssembly.RuntimeError, /indirect call signature mismatch/);

var ins = new WebAssembly.Instance(new WebAssembly.Module(wasm));
check_stub2: {
  let options = getJitCompilerOptions();
  if (!options["baseline.enable"]) break check_stub2;
  ins.exports.g.value = 1;
  for (let i = options["baseline.warmup.trigger"] + 1; i--;)
      check_err();
  ins.exports.g.value = 0;
  assertErrorMessage(check_err, WebAssembly.RuntimeError, /indirect call signature mismatch/);
}

var ins = new WebAssembly.Instance(new WebAssembly.Module(wasm));
check_stub3: {
  let options = getJitCompilerOptions();
  if (!options["ion.enable"]) break check_stub3;
  ins.exports.g.value = 1;
  var check_err2 = function() { for (var i = 0; i < 5; i++) ins.exports.test(); };
  for (let i = options["ion.warmup.trigger"]+2; i--;)
      check_err2();
  ins.exports.g.value = 0;
  assertErrorMessage(() => check_err2(), WebAssembly.RuntimeError, /indirect call signature mismatch/);
}

// Performance and stack growth: calculating sum of numbers 1..40000000
var ins = wasmEvalText(`(module
  (table 1 1 funcref)
  (type $rec (func (param i32 i64) (result i64)))
  (func $sum (param i32 i64) (result i64)
    local.get 0
    i32.eqz
    if
      local.get 1
      return
    else
      local.get 0
      i32.const 1
      i32.sub
      local.get 1
      local.get 0
      i64.extend_i32_s
      i64.add
      i32.const 0
      return_call_indirect (type $rec)
    end
    unreachable
  )
  (elem (i32.const 0) $sum)

  (func (export "main") (param i32) (result i64)
    local.get 0
    i64.const 0
    i32.const 0
    return_call_indirect (type $rec)
  )
)`);

if (getBuildConfiguration("simulator")) {
  assertEq(ins.exports.main(400000), 80000200000n);
} else {
  assertEq(ins.exports.main(40000000), 800000020000000n);
}
