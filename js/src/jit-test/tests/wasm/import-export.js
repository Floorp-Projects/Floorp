load(libdir + 'wasm.js');
load(libdir + 'asserts.js');

const Module = WebAssembly.Module;
const Instance = WebAssembly.Instance;

// Explicitly opt into the new binary format for imports and exports until it
// is used by default everywhere.
const textToBinary = str => wasmTextToBinary(str, 'new-format');

const m1 = new Module(textToBinary('(module (import "foo" "bar") (import "baz" "quux"))'));
assertErrorMessage(() => new Instance(m1), TypeError, /no import object given/);

// Import order:

var arr = [];
var importObj = {
    get foo() { arr.push("foo") },
    get baz() { arr.push("bad") },
};
assertErrorMessage(() => new Instance(m1, importObj), TypeError, /import object field is not an Object/);
assertEq(arr.join(), "foo");

var arr = [];
var importObj = {
    get foo() {
        arr.push("foo");
        return { get bar() { arr.push("bar"); return null } }
    },
    get baz() { arr.push("bad") },
};
assertErrorMessage(() => new Instance(m1, importObj), TypeError, /import object field is not a Function/);
assertEq(arr.join(), "foo,bar");

var arr = [];
var importObj = {
    get foo() {
        arr.push("foo");
        return { get bar() { arr.push("bar"); return () => arr.push("bad") } }
    },
    get baz() {
        arr.push("baz");
        return { get quux() { arr.push("quux"); return () => arr.push("bad") } }
    }
};
assertEq(new Instance(m1, importObj) instanceof Instance, true);
assertEq(arr.join(), "foo,bar,baz,quux");

// Export key order:

var code = textToBinary('(module)');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).length, 0);

var code = textToBinary('(module (func) (export "foo" 0))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "foo");
assertEq(e.foo(), undefined);

var code = textToBinary('(module (func) (export "foo" 0) (export "bar" 0))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "foo,bar");
assertEq(e.foo(), undefined);
assertEq(e.bar(), undefined);
assertEq(e.foo, e.bar);

var code = textToBinary('(module (memory 1 1) (export "memory" memory))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "memory");

var code = textToBinary('(module (memory 1 1) (export "foo" memory) (export "bar" memory))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "foo,bar");
assertEq(e.foo, e.bar);
assertEq(e.foo.buffer.byteLength, 64*1024);

var code = textToBinary('(module (memory 1 1) (func) (export "foo" 0) (export "bar" memory))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "foo,bar");
assertEq(e.foo(), undefined);
assertEq(e.bar.buffer.byteLength, 64*1024);

var code = textToBinary('(module (memory 1 1) (func) (export "bar" memory) (export "foo" 0))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "bar,foo");
assertEq(e.foo(), undefined);
assertEq(e.bar.buffer.byteLength, 64*1024);
