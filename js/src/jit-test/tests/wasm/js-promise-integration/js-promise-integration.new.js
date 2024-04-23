// New version of JS promise integration API
// Modified https://github.com/WebAssembly/js-promise-integration/tree/main/test/js-api/js-promise-integration

var tests = Promise.resolve();
function test(fn, n) {

  tests = tests.then(() => {
    let t = {res: null};
    print("# " + n);
    fn(t);
    return t.res;
  });
}
function promise_test(fn, n) {
  tests = tests.then(() => {
    print("# " + n);
    return fn();
  });
}
function assert_true(f) { assertEq(f, true); }
function assert_equals(a, b) { assertEq(a, b); }
function assert_array_equals(a, b) {
  assert_equals(a.length, a.length);
  for (let i = 0; i < a.length; i++) {
    assert_equals(a[i], b[i]);
  }
}
function assert_throws(ex, fn) {
  try {
    fn(); assertEq(false, true);
  } catch(e) {
    assertEq(e instanceof ex, true);
  }
}
function promise_rejects(t, obj, p) {
  t.res = p.then(() => {
    assertEq(true, false);
  }, (e) => {
    assertEq(e instanceof obj.constructor, true);
  });
}

function ToPromising(wasm_export) {
  let sig = wasm_export.type();
  assert_true(sig.parameters.length > 0);
  assert_equals('externref', sig.parameters[0]);
  let wrapper_sig = {
    parameters: sig.parameters.slice(1),
    results: ['externref']
  };
  return new WebAssembly.Function(
      wrapper_sig, wasm_export, {promising: 'first'});
}

promise_test(async () => {
  let js_import = WebAssembly.suspending(
      () => Promise.resolve(42)
  );
  let instance = wasmEvalText(`(module
    (import "m" "import" (func $import (result i32)))
    (func (export "test") (result i32)
      call $import
    )
  )`, {m: {import: js_import}});
  let wrapped_export = WebAssembly.promising(instance.exports.test);
  let export_promise = wrapped_export();
  assert_true(export_promise instanceof Promise);
  assert_equals(42, await export_promise);
}, "Suspend once");

promise_test(async () => {
  let i = 0;
  function js_import() {
    return Promise.resolve(++i);
  };
  let wasm_js_import = WebAssembly.suspending(js_import);
  // void test() {
  //   for (i = 0; i < 5; ++i) {
  //     g = g + await import();
  //   }
  // }
  let instance = wasmEvalText(`(module
    (import "m" "import" (func $import (param externref) (result i32)))
    (global (export "g") (mut i32) (i32.const 0))
    (func (export "test") (param externref)
      (local i32)
      i32.const 5
      local.set 1
      loop
        local.get 0
        call $import
        global.get 0
        i32.add
        global.set 0
        local.get 1
        i32.const 1
        i32.sub
        local.tee 1
        br_if 0
      end
    )
  )`, {m: {import: wasm_js_import}});
  let wrapped_export = WebAssembly.promising(instance.exports.test);
  let export_promise = wrapped_export();
  assert_equals(0, instance.exports.g.value);
  assert_true(export_promise instanceof Promise);
  await export_promise;
  assert_equals(15, instance.exports.g.value);
}, "Suspend/resume in a loop");

promise_test(async () => {
  function js_import() {
    return 42
  };
  let wasm_js_import = WebAssembly.suspending(js_import);
  let instance = wasmEvalText(`(module
    (import "m" "import" (func $import (param externref) (result i32)))
    (global (export "g") (mut i32) (i32.const 0))
    (func (export "test") (param externref) (result i32)
      local.get 0
      call $import
      global.set 0
      global.get 0
    )
  )`, {m: {import: wasm_js_import}});
  let wrapped_export = WebAssembly.promising(instance.exports.test);
  await wrapped_export();
  assert_equals(42, instance.exports.g.value);
}, "Do not suspend if the import's return value is not a Promise");

test(t => {
  let tag = new WebAssembly.Tag({parameters: []});
  function js_import() {
    return Promise.resolve();
  };
  let wasm_js_import = WebAssembly.suspending(js_import);
  function js_throw() {
    throw new Error();
  }

  let instance = wasmEvalText(`(module
    (import "m" "import" (func $import (param externref) (result i32)))
    (import "m" "js_throw" (func $js_throw))
    (func (export "test") (param externref) (result i32)
      local.get 0
      call $import
      call $js_throw
    )
  )`, {m: {import: wasm_js_import, js_throw}});
  let wrapped_export = WebAssembly.promising(instance.exports.test);
  let export_promise = wrapped_export();
  assert_true(export_promise instanceof Promise);
  promise_rejects(t, new Error(), export_promise);
}, "Throw after the first suspension");

// TODO: Use wasm exception handling to check that the exception can be caught in wasm.

test(t => {
  let tag = new WebAssembly.Tag({parameters: ['i32']});
  function js_import() {
    return Promise.reject(new Error());
  };
  let wasm_js_import = WebAssembly.suspending(js_import);

  let instance = wasmEvalText(`(module
    (import "m" "import" (func $import (param externref) (result i32)))
    (func (export "test") (param externref) (result i32)
      local.get 0
      call $import
    )
  )`, {m: {import: wasm_js_import, tag: tag}});
  let wrapped_export = WebAssembly.promising(instance.exports.test);
  let export_promise = wrapped_export();
  assert_true(export_promise instanceof Promise);
  promise_rejects(t, new Error(), export_promise);
}, "Rejecting promise");

async function TestNestedSuspenders(suspend) {
  // Nest two suspenders. The call chain looks like:
  // outer (wasm) -> outer (js) -> inner (wasm) -> inner (js)
  // If 'suspend' is true, the inner JS function returns a Promise, which
  // suspends the inner wasm function, which returns a Promise, which suspends
  // the outer wasm function, which returns a Promise. The inner Promise
  // resolves first, which resumes the inner continuation. Then the outer
  // promise resolves which resumes the outer continuation.
  // If 'suspend' is false, the inner and outer JS functions return a regular
  // value and no computation is suspended.

  let inner = WebAssembly.suspending(
      () => suspend ? Promise.resolve(42) : 43,
  );

  let export_inner;
  let outer = WebAssembly.suspending(
    () => suspend ? export_inner() : 42,
  );

  let instance = wasmEvalText(`(module
    (import "m" "inner" (func $inner (param externref) (result i32)))
    (import "m" "outer" (func $outer (param externref) (result i32)))
    (func (export "outer") (param externref) (result i32)
      local.get 0
      call $outer
    )
    (func (export "inner") (param externref) (result i32)
      local.get 0
      call $inner
    )
  )`, {m: {inner, outer}});
  export_inner = WebAssembly.promising(instance.exports.inner);
  let export_outer = WebAssembly.promising(instance.exports.outer);
  let result = export_outer();
  assert_true(result instanceof Promise);
  assert_equals(42, await result);
}

promise_test(async () => {
  return TestNestedSuspenders(true);
}, "Test nested suspenders with suspension");

promise_test(async () => {
  return TestNestedSuspenders(false);
}, "Test nested suspenders with no suspension");


test(t => {
  let instance = wasmEvalText(`(module
    (func (export "test") (result i32)
      call 0
    )
  )`);
  let wrapper = WebAssembly.promising(instance.exports.test);
  promise_rejects(t, new InternalError(), wrapper());
}, "Stack overflow");

// TODO: Test suspension with funcref.

test(t => {
  // The call stack of this test looks like:
  // export1 -> import1 -> export2 -> import2
  // Where export1 is "promising" and import2 is "suspending". Returning a
  // promise from import2 should trap because of the JS import in the middle.
  let instance;
  function import1() {
    // import1 -> export2 (unwrapped)
    instance.exports.export2();
  }
  function import2() {
    return Promise.resolve(0);
  }
  import2 = WebAssembly.suspending(import2);
  instance = wasmEvalText(`(module
    (import "m" "import1" (func $import1 (result i32)))
    (import "m" "import2" (func $import2 (result i32)))
    (func (export "export1") (result i32)
      ;; export1 -> import1 (unwrapped)
      call $import1
    )
    (func (export "export2") (result i32)
      ;; export2 -> import2 (suspending)
      call $import2
    )
  )`,
  {'m': {'import1': import1, 'import2': import2}});
  // export1 (promising)
  let wrapper = WebAssembly.promising(instance.exports.export1);
  promise_rejects(t, new WebAssembly.RuntimeError(), wrapper());
}, "Test that trying to suspend JS frames traps");

tests.then(() => print('Done'));
