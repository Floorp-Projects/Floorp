"use strict";

/* Copyright 2021 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

if (!wasmIsSupported()) {
  quit();
}

function bytes(type, bytes) {
  var typedBuffer = new Uint8Array(bytes);
  return wasmGlobalFromArrayBuffer(type, typedBuffer.buffer);
}
function value(type, value) {
  return new WebAssembly.Global({
    value: type,
    mutable: false,
  }, value);
}

function i8x16(elements) {
  let typedBuffer = new Uint8Array(elements);
  return wasmGlobalFromArrayBuffer("v128", typedBuffer.buffer);
}
function i16x8(elements) {
  let typedBuffer = new Uint16Array(elements);
  return wasmGlobalFromArrayBuffer("v128", typedBuffer.buffer);
}
function i32x4(elements) {
  let typedBuffer = new Uint32Array(elements);
  return wasmGlobalFromArrayBuffer("v128", typedBuffer.buffer);
}
function i64x2(elements) {
  let typedBuffer = new BigUint64Array(elements);
  return wasmGlobalFromArrayBuffer("v128", typedBuffer.buffer);
}
function f32x4(elements) {
  let typedBuffer = new Float32Array(elements);
  return wasmGlobalFromArrayBuffer("v128", typedBuffer.buffer);
}
function f64x2(elements) {
  let typedBuffer = new Float64Array(elements);
  return wasmGlobalFromArrayBuffer("v128", typedBuffer.buffer);
}

function either(...arr) {
  return new EitherVariants(arr);
}

class F32x4Pattern {
  constructor(x, y, z, w) {
    this.x = x;
    this.y = y;
    this.z = z;
    this.w = w;
  }
}

class F64x2Pattern {
  constructor(x, y) {
    this.x = x;
    this.y = y;
  }
}

class RefWithType {
  constructor(type) {
    this.type = type;
  }

  formatExpected() {
    return `RefWithType(${this.type})`;
  }

  test(refGlobal) {
    try {
      new WebAssembly.Global({value: this.type}, refGlobal.value);
      return true;
    } catch (err) {
      assertEq(err instanceof TypeError, true, `wrong type of error when creating global: ${err}`);
      assertEq(!!err.message.match(/can only pass/), true, `wrong type of error when creating global: ${err}`);
      return false;
    }
  }
}

// ref.extern values created by spec tests will be JS objects of the form
// { [externsym]: <number> }. Other externref values are possible to observe
// if extern.externalize is used.
let externsym = Symbol("externref");
function externref(s) {
  return { [externsym]: s };
}
function is_externref(x) {
  return (x !== null && externsym in x) ? 1 : 0;
}
function is_funcref(x) {
  return typeof x === "function" ? 1 : 0;
}
function eq_externref(x, y) {
  return x === y ? 1 : 0;
}
function eq_funcref(x, y) {
  return x === y ? 1 : 0;
}

class ExternRefResult {
  constructor(n) {
    this.n = n;
  }

  formatExpected() {
    return `ref.extern ${this.n}`;
  }

  test(global) {
    // the global's value can either be an externref or just a plain old JS number
    let result = global.value;
    if (typeof global.value === "object" && externsym in global.value) {
      result = global.value[externsym];
    }
    return result === this.n;
  }
}

// ref.host values created by spectests will be whatever the JS API does to
// convert the given value to anyref. It should implicitly be like extern.internalize.
function hostref(v) {
  if (!wasmGcEnabled()) {
    throw new Error("ref.host only works when wasm GC is enabled");
  }

  const { internalizeNum } = new WebAssembly.Instance(
    new WebAssembly.Module(wasmTextToBinary(`(module
      (func (import "test" "coerce") (param i32) (result anyref))
      (func (export "internalizeNum") (param i32) (result anyref)
        (call 0 (local.get 0))
      )
    )`)),
    { "test": { "coerce": x => x } },
  ).exports;
  return internalizeNum(v);
}

class HostRefResult {
  constructor(n) {
    this.n = n;
  }

  formatExpected() {
    return `ref.host ${this.n}`;
  }

  test(externrefGlobal) {
    assertEq(externsym in externrefGlobal.value, true, `HostRefResult only works with externref inputs`);
    return externrefGlobal.value[externsym] === this.n;
  }
}

let spectest = {
  externref: externref,
  is_externref: is_externref,
  is_funcref: is_funcref,
  eq_externref: eq_externref,
  eq_funcref: eq_funcref,
  print: console.log.bind(console),
  print_i32: console.log.bind(console),
  print_i32_f32: console.log.bind(console),
  print_f64_f64: console.log.bind(console),
  print_f32: console.log.bind(console),
  print_f64: console.log.bind(console),
  global_i32: 666,
  global_i64: 666n,
  global_f32: 666,
  global_f64: 666,
  table: new WebAssembly.Table({
    initial: 10,
    maximum: 20,
    element: "anyfunc",
  }),
  memory: new WebAssembly.Memory({ initial: 1, maximum: 2 }),
};

let linkage = {
  spectest,
};

function getInstance(instanceish) {
  if (typeof instanceish === "string") {
    assertEq(
      instanceish in linkage,
      true,
      `'${instanceish}'' must be registered`,
    );
    return linkage[instanceish];
  }
  return instanceish;
}

function instantiate(source) {
  let bytecode = wasmTextToBinary(source);
  let module = new WebAssembly.Module(bytecode);
  let instance = new WebAssembly.Instance(module, linkage);
  return instance.exports;
}

function register(instanceish, name) {
  linkage[name] = getInstance(instanceish);
}

function invoke(instanceish, field, params) {
  let func = getInstance(instanceish)[field];
  assertEq(func instanceof Function, true, "expected a function");
  return wasmLosslessInvoke(func, ...params);
}

function get(instanceish, field) {
  let global = getInstance(instanceish)[field];
  assertEq(
    global instanceof WebAssembly.Global,
    true,
    "expected a WebAssembly.Global",
  );
  return global;
}

function assert_trap(thunk, message) {
  try {
    thunk();
    throw new Error("expected trap");
  } catch (err) {
    if (err instanceof WebAssembly.RuntimeError) {
      return;
    }
    throw err;
  }
}

let StackOverflow;
try {
  (function f() {
    1 + f();
  })();
} catch (e) {
  StackOverflow = e.constructor;
}
function assert_exhaustion(thunk, message) {
  try {
    thunk();
    assertEq("normal return", "exhaustion");
  } catch (err) {
    assertEq(
      err instanceof StackOverflow,
      true,
      "expected exhaustion",
    );
  }
}

function assert_invalid(thunk, message) {
  try {
    thunk();
    assertEq("valid module", "invalid module");
  } catch (err) {
    assertEq(
      err instanceof WebAssembly.LinkError ||
        err instanceof WebAssembly.CompileError,
      true,
      "expected an invalid module",
    );
  }
}

function assert_unlinkable(thunk, message) {
  try {
    thunk();
    assertEq(true, false, "expected an unlinkable module");
  } catch (err) {
    assertEq(
      err instanceof WebAssembly.LinkError ||
        err instanceof WebAssembly.CompileError,
      true,
      "expected an unlinkable module",
    );
  }
}

function assert_malformed(thunk, message) {
  try {
    thunk();
    assertEq("valid module", "malformed module");
  } catch (err) {
    assertEq(
      err instanceof TypeError ||
        err instanceof SyntaxError ||
        err instanceof WebAssembly.CompileError ||
        err instanceof WebAssembly.LinkError,
      true,
      `expected a malformed module`,
    );
  }
}

function assert_exception(thunk) {
  let thrown = false;
  try {
    thunk();
  } catch (err) {
    thrown = true;
  }
  assertEq(thrown, true, "expected an exception to be thrown");
}

function assert_return(thunk, expected) {
  let results = thunk();

  if (results === undefined) {
    results = [];
  } else if (!Array.isArray(results)) {
    results = [results];
  }
  if (!Array.isArray(expected)) {
    expected = [expected];
  }

  if (!compareResults(results, expected)) {
    let got = results.map((x) => formatResult(x)).join(", ");
    let wanted = expected.map((x) => formatExpected(x)).join(", ");
    assertEq(
      `[${got}]`,
      `[${wanted}]`,
    );
    assertEq(true, false, `${got} !== ${wanted}`);
  }
}

function formatResult(result) {
  if (typeof (result) === "object") {
    return wasmGlobalToString(result);
  } else {
    return `${result}`;
  }
}

function formatExpected(expected) {
  if (
    expected === `f32_canonical_nan` ||
    expected === `f32_arithmetic_nan` ||
    expected === `f64_canonical_nan` ||
    expected === `f64_arithmetic_nan`
  ) {
    return expected;
  } else if (expected instanceof F32x4Pattern) {
    return `f32x4(${formatExpected(expected.x)}, ${
      formatExpected(expected.y)
    }, ${formatExpected(expected.z)}, ${formatExpected(expected.w)})`;
  } else if (expected instanceof F64x2Pattern) {
    return `f64x2(${formatExpected(expected.x)}, ${
      formatExpected(expected.y)
    })`;
  } else if (expected instanceof EitherVariants) {
    return expected.formatExpected();
  } else if (expected instanceof RefWithType) {
    return expected.formatExpected();
  } else if (expected instanceof ExternRefResult) {
    return expected.formatExpected();
  } else if (expected instanceof HostRefResult) {
    return expected.formatExpected();
  } else if (typeof (expected) === "object") {
    return wasmGlobalToString(expected);
  } else {
    throw new Error("unknown expected result");
  }
}

class EitherVariants {
  constructor(arr) {
    this.arr = arr;
  }
  matches(v) {
    return this.arr.some((e) => compareResult(v, e));
  }
  formatExpected() {
    return `either(${this.arr.map(formatExpected).join(", ")})`;
  }
}

function compareResults(results, expected) {
  if (results.length !== expected.length) {
    return false;
  }
  for (let i in results) {
    if (expected[i] instanceof EitherVariants) {
      return expected[i].matches(results[i]);
    }
    if (!compareResult(results[i], expected[i])) {
      return false;
    }
  }
  return true;
}

function compareResult(result, expected) {
  if (
    expected === `canonical_nan` ||
    expected === `arithmetic_nan`
  ) {
    return wasmGlobalIsNaN(result, expected);
  } else if (expected === null) {
    return result.value === null;
  } else if (expected instanceof F32x4Pattern) {
    return compareResult(
      wasmGlobalExtractLane(result, "f32x4", 0),
      expected.x,
    ) &&
      compareResult(wasmGlobalExtractLane(result, "f32x4", 1), expected.y) &&
      compareResult(wasmGlobalExtractLane(result, "f32x4", 2), expected.z) &&
      compareResult(wasmGlobalExtractLane(result, "f32x4", 3), expected.w);
  } else if (expected instanceof F64x2Pattern) {
    return compareResult(
      wasmGlobalExtractLane(result, "f64x2", 0),
      expected.x,
    ) &&
      compareResult(wasmGlobalExtractLane(result, "f64x2", 1), expected.y);
  } else if (expected instanceof RefWithType) {
    return expected.test(result);
  } else if (expected instanceof ExternRefResult) {
    return expected.test(result);
  } else if (expected instanceof HostRefResult) {
    return expected.test(result);
  } else if (typeof (expected) === "object") {
    return wasmGlobalsEqual(result, expected);
  } else {
    throw new Error("unknown expected result");
  }
}
