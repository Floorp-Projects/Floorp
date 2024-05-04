// New API experiments.
// Example from the proposal.

var compute_delta = async (i) => Promise.resolve(i / 100 || 1);

var suspending_compute_delta = new WebAssembly.Suspending(
    compute_delta
);
var ins = wasmEvalText(`(module
  (import "js" "init_state" (func $init_state (result f64)))
  (import "js" "compute_delta"
    (func $compute_delta (param i32) (result f64)))

  (global $suspender (mut externref) (ref.null extern))
  (global $state (mut f64) (f64.const nan))
  (func $init (global.set $state (call $init_state)))
  (start $init)

  (func $get_state (export "get_state") (result f64) (global.get $state))
  (func (export "update_state_export") (param i32) (result f64)
    (global.set $state (f64.add
      (global.get $state) (call $compute_delta (local.get 0))))
    (global.get $state)
  )
)`, {
    js: {
        init_state() { return 0; },
        compute_delta: suspending_compute_delta,
    },
});

var update_state = WebAssembly.promising(
    ins.exports.update_state_export
);

var res = update_state(4);
var tasks = res.then((r) => {
    print(r);
    assertEq(ins.exports.get_state(), .04);
});

assertEq(ins.exports.get_state(), 0);

// Also test with exceptions/traps.

async function test(c) {
  var compute_delta = (i) => Promise.resolve(i/100 || 1);
  if (c == 1) compute_delta = async (i) => {throw "ff"};
  if (c == 2) compute_delta = () => {throw "ff";}

  var suspending_compute_delta = new WebAssembly.Suspending(
    compute_delta
  );
  var ins = wasmEvalText(`(module
    (import "js" "init_state" (func $init_state (result f64)))
    (import "js" "compute_delta"
      (func $compute_delta (param i32) (result f64)))

    (global $suspender (mut externref) (ref.null extern))
    (global $state (mut f64) (f64.const nan))
    (func $init (global.set $state (call $init_state)))
    (start $init)

    (func $get_state (export "get_state") (result f64) (global.get $state))
    (func (export "update_state_export") (param i32) (result f64)
    ${c == 3 ? "(unreachable)" : ""}
      (global.set $state (f64.add
        (global.get $state) (call $compute_delta (local.get 0))))
    ${c == 4 ? "(unreachable)" : ""}
      (global.get $state)
    )
  )`, {
    js: {
      init_state() { return 0; },
      compute_delta: suspending_compute_delta,
    },
  });

  var update_state = WebAssembly.promising(
    ins.exports.update_state_export
);

  var res = update_state(4);
  var p = res.then((r) => {
    assertEq(c, 0);
    assertEq(ins.exports.get_state(), .04);
  }).catch(_ => {
    assertEq(c > 0, true);
  });

  assertEq(ins.exports.get_state(), 0);
  await p;
}

for (let c = 0; c < 5; c++) {
  tasks = tasks.then(() => test(c));
}
