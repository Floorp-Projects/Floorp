// Example from the proposal.

gczeal(2,5);

var compute_delta = (i) => Promise.resolve(i / 100 || 1);

var suspending_compute_delta = new WebAssembly.Function(
    { parameters: ['externref', 'i32'], results: ['f64'] },
    compute_delta,
    { suspending: "first" }
);

var ins = wasmEvalText(`(module
    (import "js" "init_state" (func $init_state (result f64)))
    (import "js" "compute_delta"
      (func $compute_delta_import (param externref) (param i32) (result f64)))

    (global $suspender (mut externref) (ref.null extern))
    (global $state (mut f64) (f64.const nan))
    (func $init (global.set $state (call $init_state)))
    (start $init)

    (func $compute_delta (param i32) (result f64)
      (local $suspender_copy externref)
      (;return  (f64.const 0.3);)
      (;unreachable;)
      (global.get $suspender)
      (local.tee $suspender_copy)
      (local.get 0)
      (call $compute_delta_import)
      (local.get $suspender_copy)
      (global.set $suspender)
      (return)
    )
    (func $get_state (export "get_state") (result f64) (global.get $state))
    (func $update_state (param i32) (result f64)
      (global.set $state (f64.add
        (global.get $state) (call $compute_delta (local.get 0))))
      (global.get $state)
    )

    (func (export "update_state_export")
      (param $suspender externref) (param i32) (result f64)
      (local.get $suspender)
      (global.set $suspender)
      (local.get 1)
      (call $update_state)
      (return)
    )
)`, {
    js: {
        init_state() { return 0; },
        compute_delta: suspending_compute_delta,
    },
});

var update_state = new WebAssembly.Function(
    { parameters: ['i32'], results: ['externref'] },
    ins.exports.update_state_export,
    { promising: "first" }
);

var res = update_state(4);
var tasks = res.then((r) => {
    print(r);
    assertEq(ins.exports.get_state(), .04);
});

assertEq(ins.exports.get_state(), 0);
