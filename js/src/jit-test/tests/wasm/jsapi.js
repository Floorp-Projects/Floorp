load(libdir + 'wasm.js');
load(libdir + 'asserts.js');

const emptyModule = wasmTextToBinary('(module)');

// 'WebAssembly' property on global object
const wasmDesc = Object.getOwnPropertyDescriptor(this, 'WebAssembly');
assertEq(typeof wasmDesc.value, "object");
assertEq(wasmDesc.writable, true);
assertEq(wasmDesc.enumerable, false);
assertEq(wasmDesc.configurable, true);

// 'WebAssembly' object
assertEq(WebAssembly, wasmDesc.value);
assertEq(String(WebAssembly), "[object WebAssembly]");

// 'WebAssembly.Module' property
const moduleDesc = Object.getOwnPropertyDescriptor(WebAssembly, 'Module');
assertEq(typeof moduleDesc.value, "function");
assertEq(moduleDesc.writable, true);
assertEq(moduleDesc.enumerable, false);
assertEq(moduleDesc.configurable, true);

// 'WebAssembly.Module' constructor function
const Module = WebAssembly.Module;
assertEq(Module, moduleDesc.value);
assertEq(Module.length, 1);
assertEq(Module.name, "Module");
assertErrorMessage(() => Module(), TypeError, /constructor without new is forbidden/);
assertErrorMessage(() => new Module(1), TypeError, "first argument must be an ArrayBuffer or typed array object");
assertErrorMessage(() => new Module({}), TypeError, "first argument must be an ArrayBuffer or typed array object");
assertErrorMessage(() => new Module(new Uint8Array()), /* TODO: WebAssembly.CompileError */ TypeError, /wasm validation error/);
assertErrorMessage(() => new Module(new ArrayBuffer()), /* TODO: WebAssembly.CompileError */ TypeError, /wasm validation error/);
assertEq(new Module(emptyModule) instanceof Module, true);
assertEq(new Module(emptyModule.buffer) instanceof Module, true);

// 'WebAssembly.Module.prototype' property
const moduleProtoDesc = Object.getOwnPropertyDescriptor(Module, 'prototype');
assertEq(typeof moduleProtoDesc.value, "object");
assertEq(moduleProtoDesc.writable, false);
assertEq(moduleProtoDesc.enumerable, false);
assertEq(moduleProtoDesc.configurable, false);

// 'WebAssembly.Module.prototype' object
const moduleProto = Module.prototype;
assertEq(moduleProto, moduleProtoDesc.value);
assertEq(String(moduleProto), "[object Object]");
assertEq(Object.getPrototypeOf(moduleProto), Object.prototype);

// 'WebAssembly.Module' instance objects
const m1 = new Module(emptyModule);
assertEq(typeof m1, "object");
assertEq(String(m1), "[object WebAssembly.Module]");
assertEq(Object.getPrototypeOf(m1), moduleProto);

// 'WebAssembly.Instance' property
const instanceDesc = Object.getOwnPropertyDescriptor(WebAssembly, 'Instance');
assertEq(typeof instanceDesc.value, "function");
assertEq(instanceDesc.writable, true);
assertEq(instanceDesc.enumerable, false);
assertEq(instanceDesc.configurable, true);

// 'WebAssembly.Instance' constructor function
const Instance = WebAssembly.Instance;
assertEq(Instance, instanceDesc.value);
assertEq(Instance.length, 1);
assertEq(Instance.name, "Instance");
assertErrorMessage(() => Instance(), TypeError, /constructor without new is forbidden/);
assertErrorMessage(() => new Instance(1), TypeError, "first argument must be a WebAssembly.Module");
assertErrorMessage(() => new Instance({}), TypeError, "first argument must be a WebAssembly.Module");
assertErrorMessage(() => new Instance(m1, null), TypeError, "second argument, if present, must be an object");
assertEq(new Instance(m1) instanceof Instance, true);
assertEq(new Instance(m1, {}) instanceof Instance, true);

// 'WebAssembly.Instance.prototype' property
const instanceProtoDesc = Object.getOwnPropertyDescriptor(Instance, 'prototype');
assertEq(typeof instanceProtoDesc.value, "object");
assertEq(instanceProtoDesc.writable, false);
assertEq(instanceProtoDesc.enumerable, false);
assertEq(instanceProtoDesc.configurable, false);

// 'WebAssembly.Instance.prototype' object
const instanceProto = Instance.prototype;
assertEq(instanceProto, instanceProtoDesc.value);
assertEq(String(instanceProto), "[object Object]");
assertEq(Object.getPrototypeOf(instanceProto), Object.prototype);

// 'WebAssembly.Instance' instance objects
const i1 = new Instance(m1);
assertEq(typeof i1, "object");
assertEq(String(i1), "[object WebAssembly.Instance]");
assertEq(Object.getPrototypeOf(i1), instanceProto);

// 'WebAssembly.Instance' 'exports' property
const exportsDesc = Object.getOwnPropertyDescriptor(i1, 'exports');
assertEq(typeof exportsDesc.value, "object");
assertEq(exportsDesc.writable, true);
assertEq(exportsDesc.enumerable, true);
assertEq(exportsDesc.configurable, true);

// Exports object:
// Note: at some point the exports object should become an ES6 module namespace
// exotic object. For now, don't probe too hard on the property descriptors or
// the exports object itself.

const e1 = i1.exports;
assertEq(e1, exportsDesc.value);
assertEq(Object.keys(e1).length, 0);

var code = wasmTextToBinary('(module (func) (export "foo" 0))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "foo");
assertEq(e.foo(), undefined);

var code = wasmTextToBinary('(module (func) (export "foo" 0) (export "bar" 0))');
var e = new Instance(new Module(code)).exports;
assertEq(Object.keys(e).join(), "foo,bar");
assertEq(e.foo(), undefined);
assertEq(e.bar(), undefined);
assertEq(e.foo, e.bar);
