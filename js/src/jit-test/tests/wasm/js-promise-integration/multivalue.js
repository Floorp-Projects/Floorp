// Test passing of multi-value results.

var suspending_fib2 = new WebAssembly.Suspending(
  async (i, j) => { return [j, i + j] }
);

var ins = wasmEvalText(`(module
  (import "js" "fib2" (func $tt (param i32 i32) (result i32 i32)))

  (func (export "fib1") (param i32 i32) (result i32 i32)
    local.get 1
    local.get 1
    local.get 0
    i32.add
    call $tt
  )
)`, {
  js: {
    fib2: suspending_fib2,
  },
});

var fib = WebAssembly.promising(ins.exports.fib1);

var res = fib(1, 1);
res.then((r) => {
  assertEq(r instanceof Array, true);
  assertEq(r.join(","), "2,3");
}).catch(e => {
  assertEq(true, false);
});


// Test passing of multi-value results (old API version).

var suspending_fib2 = new WebAssembly.Function(
  { parameters: ['externref', 'i32', 'i32'], results: ['i32', 'i32'] },
  async (i, j) => { return [j, i + j] },
  { suspending: 'first' },
);

var ins = wasmEvalText(`(module
  (import "js" "fib2" (func $tt (param externref i32 i32) (result i32 i32)))

  (func (export "fib1") (param externref i32 i32) (result i32 i32)
    local.get 0
    local.get 2
    local.get 2
    local.get 1
    i32.add
    call $tt
  )
)`, {
  js: {
    fib2: suspending_fib2,
  },
});

var fib = new WebAssembly.Function(
  {
      parameters: ['i32', 'i32'],
      results: ['externref']
  }, ins.exports.fib1, { promising: 'first' },);

var res = fib(1, 2);
res.then((r) => {
  assertEq(r instanceof Array, true);
  assertEq(r.join(","), "3,5");
}).catch(e => {
  assertEq(true, false);
});


// Test no results returned.

var check = 0;
var ins = wasmEvalText(`(module
  (import "js" "out" (func $out (param i32)))

  (func (export "in") (param i32)
    local.get 0
    call $out
  )
)`, {
  js: {
    out: new WebAssembly.Suspending(async(i) => {
      check += i;
      return 42;
    }),
  },
});

var res = WebAssembly.promising(ins.exports.in)(1);
res.then((r) => {
  assertEq(typeof r, "undefined");
  assertEq(check, 1);
}).catch(e => {
  assertEq(true, false);
});

// Check non-iterable returned from JS.
var ins  = wasmEvalText(`(module
  (import "js" "fail" (func $out (result i32 i32)))

  (func (export "test") (result i32)
    call $out
    i32.add
  )
)`, {
  js: {
    fail: new WebAssembly.Suspending(() => 42),
  },
});

var res = WebAssembly.promising(ins.exports.test)();
res.then((r) => {
  assertEq(true, false);
}).catch(e => {
  assertEq(e instanceof TypeError, true)
});

// Check wrong number of values returned from JS.
var ins  = wasmEvalText(`(module
  (import "js" "fail" (func $out (result i32 i32 i32)))

  (func (export "test") (result i32 i32)
    call $out
    i32.add
  )
)`, {
  js: {
    fail: new WebAssembly.Suspending(() => [42, 1]),
  },
});

var res = WebAssembly.promising(ins.exports.test)();
res.then((r) => {
  assertEq(true, false);
}).catch(e => {
  assertEq(e instanceof TypeError, true)
});
