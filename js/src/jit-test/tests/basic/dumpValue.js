// |jit-test| skip-if: typeof dumpValue !== 'function'

// Try the dumpValue and dumpValueToString shell functions on various types of
// values, and make sure theyit don't crash, and the result is valid JSON.

function testDump(v) {
  dumpValue(v);

  const s = dumpValueToString(v);

  const result = JSON.parse(s);
  assertEq(typeof result, "object");
  assertEq(typeof result.type, "string");
}


testDump(1);
testDump(1.1);
testDump(-0.1);

testDump(100n);

testDump(true);
testDump(false);

testDump(null);

testDump(undefined);

// dumpStringRepresentation.js covers more strings.
testDump("foo");

testDump(/foo/ig);

testDump(Symbol.iterator);
testDump(Symbol("hello"));
testDump(Symbol.for("hello"));

testDump({});
testDump({ prop1: 10, prop2: 20 });

testDump([]);
testDump([1, , 3, 4]);

testDump(function f() {});
testDump(function* f() {});
testDump(async function f() {});
testDump(async function* f() {});

testDump(Promise.withResolvers());

var p1 = new Promise(() => {}); p1.then(() => {});
testDump(p1);
var p2 = new Promise(() => {}); p2.then(() => {}); p2.then(() => {});
testDump(p2);
var p3 = Promise.reject(10).catch(() => {});
testDump(p3);

testDump(new ArrayBuffer([1, 2, 3]));
testDump(new Int8Array([1, 2, 3]));
testDump(new Int8Array(new Int8Array([1, 2, 3]).buffer, 1));
testDump(new Int32Array([1, 2, 3]));
testDump(new Int32Array(new Int32Array([1, 2, 3]).buffer, 4));
testDump(new Float64Array([1, 2, 3]));

testDump(new Date());
testDump(new Map([[1, 2]]));
testDump(new Set([1, 2]));
testDump(new WeakMap([ [{}, 10], [{}, 20] ]));
testDump(new WeakSet([{}, {}]));
testDump(new Proxy({}, {}));

testDump(Array);
testDump(Array.prototype);
testDump(this);

testDump([
  1,
  1.1,
  -0.1,

  100n,

  true,
  false,

  null,

  undefined,

  "foo",

  /foo/ig,

  Symbol.iterator,
  Symbol("hello"),
  Symbol.for("hello"),

  {},
  { prop1: 10, prop2: 20 },

  [],
  [1, , 3, 4],

  function f() {},
  function* f() {},
  async function f() {},
  async function* f() {},

  Promise.withResolvers(),
  p1,
  p2,
  p3,

  new ArrayBuffer([1, 2, 3]),
  new Int8Array([1, 2, 3]),
  new Int8Array(new Int8Array([1, 2, 3]).buffer, 1),
  new Int32Array([1, 2, 3]),
  new Int32Array(new Int32Array([1, 2, 3]).buffer, 4),
  new Float64Array([1, 2, 3]),
  new Float64Array([1, 2, 3]),

  new Map([[1, 2]]),
  new Set([1, 2]),
  new WeakMap([ [{}, 10], [{}, 20] ]),
  new WeakSet([{}, {}]),
  new Proxy({}, {}),

  Array,
  Array.prototype,
  this,
]);
