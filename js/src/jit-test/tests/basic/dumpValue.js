// |jit-test| skip-if: typeof dumpValue !== 'function'

// Try the dumpValue shell function on various types of values, and make sure
// it doesn't crash.

dumpValue(1);
dumpValue(1.1);
dumpValue(-0.1);

dumpValue(100n);

dumpValue(true);
dumpValue(false);

dumpValue(null);

dumpValue(undefined);

// dumpStringRepresentation.js covers more strings.
dumpValue("foo");

dumpValue(/foo/ig);

dumpValue(Symbol.iterator);
dumpValue(Symbol("hello"));
dumpValue(Symbol.for("hello"));

dumpValue({});
dumpValue({ prop1: 10, prop2: 20 });

dumpValue([]);
dumpValue([1, , 3, 4]);

dumpValue(function f() {});
dumpValue(function* f() {});
dumpValue(async function f() {});
dumpValue(async function* f() {});

dumpValue(Promise.withResolvers());

var p1 = new Promise(() => {}); p1.then(() => {});
dumpValue(p1);
var p2 = new Promise(() => {}); p2.then(() => {}); p2.then(() => {});
dumpValue(p2);
var p3 = Promise.reject(10).catch(() => {});
dumpValue(p3);

dumpValue(new ArrayBuffer([1, 2, 3]));
dumpValue(new Int8Array([1, 2, 3]));
dumpValue(new Int8Array(new Int8Array([1, 2, 3]).buffer, 1));
dumpValue(new Int32Array([1, 2, 3]));
dumpValue(new Int32Array(new Int32Array([1, 2, 3]).buffer, 4));
dumpValue(new Float64Array([1, 2, 3]));

dumpValue(new Date());
dumpValue(new Map([[1, 2]]));
dumpValue(new Set([1, 2]));
dumpValue(new WeakMap([ [{}, 10], [{}, 20] ]));
dumpValue(new WeakSet([{}, {}]));
dumpValue(new Proxy({}, {}));

dumpValue(Array);
dumpValue(Array.prototype);
dumpValue(this);

dumpValue([
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
