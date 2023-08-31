// |jit-test| --wasm-gc; skip-if: !wasmGcEnabled()
var ins = wasmEvalText(`(module
    (type $t (func (param i64 i64 funcref) (result i64)))
    (elem declare func $fac-acc $fac-acc-broken)
    (func $fac-acc (export "fac-acc") (param i64 i64 funcref) (result i64)
        (if (result i64) (i64.eqz (local.get 0))
        (then (local.get 1))
        (else
            (return_call $vis
                (i64.sub (local.get 0) (i64.const 1))
                (i64.mul (local.get 0) (local.get 1))
                (local.get 2)
            )
        )
        )
    )
    ;; same as $fac-acc but fails on i == 6
    (func $fac-acc-broken (param i64 i64 funcref) (result i64)
        (if (result i64) (i64.eqz (local.get 0))
        (then (local.get 1))
        (else
            (return_call $vis
                (i64.sub (local.get 0) (i64.const 1))
                (i64.mul (local.get 0) (local.get 1))
                (select (result funcref)
                  (ref.null func) (local.get 2)
                  (i64.eq (local.get 0) (i64.const 6)))
            )
        )
        )
    )
    (func $vis (export "vis") (param i64 i64 funcref) (result i64)
        local.get 0
        local.get 1
        local.get 2
        local.get 2
        ref.cast (ref null $t)
        return_call_ref $t
    )
    (func $trap (export "trap") (param i64 i64 funcref) (result i64)
        unreachable
    )
    (func (export "main") (param i64) (result i64)
        (call $vis (local.get 0) (i64.const 1) (ref.func $fac-acc))
    )
    (func (export "main_null") (param i64) (result i64)
        (return_call $vis (local.get 0) (i64.const 1) (ref.null $t))
    )
    (func (export "main_broken") (param i64) (result i64)
        (return_call $vis (local.get 0) (i64.const 1) (ref.func $fac-acc-broken))
    )
)`);

// Check return call via wasm function
assertEq(ins.exports.main(5n), 120n);

// Check return call directly via interpreter stub
const fac = ins.exports["fac-acc"];
const vis = ins.exports["vis"];
assertEq(vis(4n, 1n, fac), 24n);

// Calling into JavaScript (and back).
if ("Function" in WebAssembly) {
  const visFn = new WebAssembly.Function({
    parameters: ["i64", "i64", "funcref"],
    results: ["i64"]
  }, function (i, n, fn) {
    if (i <= 0n) {
      return n;
    }
    return vis(i - 1n, i * n, fn);
  });

  assertEq(vis(3n, 1n, visFn), 6n);
}

// Check return call directly via jit stub
check_stub1: {
    let options = getJitCompilerOptions();
    if (!options["baseline.enable"]) break check_stub1;
    const check = function() {
      vis(4n, 1n, fac);
    };
    for (let i = options["baseline.warmup.trigger"] + 1; i--;)
        check();
}

// Handling traps.
const trap = ins.exports["trap"];
assertErrorMessage(() => vis(4n, 1n, trap), WebAssembly.RuntimeError, /unreachable executed/);
const main_broken = ins.exports["main_broken"];
assertErrorMessage(() => main_broken(8n), WebAssembly.RuntimeError, /dereferencing null pointer/);
const main_null = ins.exports["main_null"];
assertErrorMessage(() => main_null(5n), WebAssembly.RuntimeError, /dereferencing null pointer/);
