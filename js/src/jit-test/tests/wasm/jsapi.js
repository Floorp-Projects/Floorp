load(libdir + 'wasm.js');
load(libdir + 'asserts.js');

// Explicitly opt into the new binary format for imports and exports until it
// is used by default everywhere.
const textToBinary = str => wasmTextToBinary(str, 'new-format');

const emptyModule = textToBinary('(module)');

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
assertErrorMessage(() => new Module(), TypeError, /requires more than 0 arguments/);
assertErrorMessage(() => new Module(undefined), TypeError, "first argument must be an ArrayBuffer or typed array object");
assertErrorMessage(() => new Module(1), TypeError, "first argument must be an ArrayBuffer or typed array object");
assertErrorMessage(() => new Module({}), TypeError, "first argument must be an ArrayBuffer or typed array object");
assertErrorMessage(() => new Module(new Uint8Array()), /* TODO: WebAssembly.CompileError */ TypeError, /compile error/);
assertErrorMessage(() => new Module(new ArrayBuffer()), /* TODO: WebAssembly.CompileError */ TypeError, /compile error/);
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

// TODO: test export object objects are ES6 module namespace objects.

// 'WebAssembly.Memory' property
const memoryDesc = Object.getOwnPropertyDescriptor(WebAssembly, 'Memory');
assertEq(typeof memoryDesc.value, "function");
assertEq(memoryDesc.writable, true);
assertEq(memoryDesc.enumerable, false);
assertEq(memoryDesc.configurable, true);

// 'WebAssembly.Memory' constructor function
const Memory = WebAssembly.Memory;
assertEq(Memory, memoryDesc.value);
assertEq(Memory.length, 1);
assertEq(Memory.name, "Memory");
assertErrorMessage(() => Memory(), TypeError, /constructor without new is forbidden/);
assertErrorMessage(() => new Memory(1), TypeError, "first argument must be a memory descriptor");
assertErrorMessage(() => new Memory({initial:{valueOf() { throw new Error("here")}}}), Error, "here");
assertErrorMessage(() => new Memory({initial:-1}), TypeError, /bad Memory initial size/);
assertErrorMessage(() => new Memory({initial:Math.pow(2,32)}), TypeError, /bad Memory initial size/);
assertEq(new Memory({initial:1}) instanceof Memory, true);
assertEq(new Memory({initial:1.5}).buffer.byteLength, 64*1024);

// 'WebAssembly.Memory.prototype' property
const memoryProtoDesc = Object.getOwnPropertyDescriptor(Memory, 'prototype');
assertEq(typeof memoryProtoDesc.value, "object");
assertEq(memoryProtoDesc.writable, false);
assertEq(memoryProtoDesc.enumerable, false);
assertEq(memoryProtoDesc.configurable, false);

// 'WebAssembly.Memory.prototype' object
const memoryProto = Memory.prototype;
assertEq(memoryProto, memoryProtoDesc.value);
assertEq(String(memoryProto), "[object Object]");
assertEq(Object.getPrototypeOf(memoryProto), Object.prototype);

// 'WebAssembly.Memory' instance objects
const mem1 = new Memory({initial:1});
assertEq(typeof mem1, "object");
assertEq(String(mem1), "[object WebAssembly.Memory]");
assertEq(Object.getPrototypeOf(mem1), memoryProto);

// 'WebAssembly.Memory.prototype.buffer' accessor property
const bufferDesc = Object.getOwnPropertyDescriptor(memoryProto, 'buffer');
assertEq(typeof bufferDesc.get, "function");
assertEq(bufferDesc.set, undefined);
assertEq(bufferDesc.enumerable, false);
assertEq(bufferDesc.configurable, true);

// 'WebAssembly.Memory.prototype.buffer' getter
const bufferGetter = bufferDesc.get;
assertErrorMessage(() => bufferGetter.call(), TypeError, /called on incompatible undefined/);
assertErrorMessage(() => bufferGetter.call({}), TypeError, /called on incompatible Object/);
assertEq(bufferGetter.call(mem1) instanceof ArrayBuffer, true);
assertEq(bufferGetter.call(mem1).byteLength, 64 * 1024);

// 'WebAssembly.Table' property
const tableDesc = Object.getOwnPropertyDescriptor(WebAssembly, 'Table');
assertEq(typeof tableDesc.value, "function");
assertEq(tableDesc.writable, true);
assertEq(tableDesc.enumerable, false);
assertEq(tableDesc.configurable, true);

// 'WebAssembly.Table' constructor function
const Table = WebAssembly.Table;
assertEq(Table, tableDesc.value);
assertEq(Table.length, 1);
assertEq(Table.name, "Table");
assertErrorMessage(() => Table(), TypeError, /constructor without new is forbidden/);
assertErrorMessage(() => new Table(1), TypeError, "first argument must be a table descriptor");
assertErrorMessage(() => new Table({initial:1, element:1}), TypeError, /must be "anyfunc"/);
assertErrorMessage(() => new Table({initial:1, element:"any"}), TypeError, /must be "anyfunc"/);
assertErrorMessage(() => new Table({initial:1, element:{valueOf() { return "anyfunc" }}}), TypeError, /must be "anyfunc"/);
assertErrorMessage(() => new Table({initial:{valueOf() { throw new Error("here")}}, element:"anyfunc"}), Error, "here");
assertErrorMessage(() => new Table({initial:-1, element:"anyfunc"}), TypeError, /bad Table initial size/);
assertErrorMessage(() => new Table({initial:Math.pow(2,32), element:"anyfunc"}), TypeError, /bad Table initial size/);
assertEq(new Table({initial:1, element:"anyfunc"}) instanceof Table, true);
assertEq(new Table({initial:1.5, element:"anyfunc"}) instanceof Table, true);

// 'WebAssembly.Table.prototype' property
const tableProtoDesc = Object.getOwnPropertyDescriptor(Table, 'prototype');
assertEq(typeof tableProtoDesc.value, "object");
assertEq(tableProtoDesc.writable, false);
assertEq(tableProtoDesc.enumerable, false);
assertEq(tableProtoDesc.configurable, false);

// 'WebAssembly.Table.prototype' object
const tableProto = Table.prototype;
assertEq(tableProto, tableProtoDesc.value);
assertEq(String(tableProto), "[object Object]");
assertEq(Object.getPrototypeOf(tableProto), Object.prototype);

// 'WebAssembly.Table' instance objects
const tbl1 = new Table({initial:2, element:"anyfunc"});
assertEq(typeof tbl1, "object");
assertEq(String(tbl1), "[object WebAssembly.Table]");
assertEq(Object.getPrototypeOf(tbl1), tableProto);

// 'WebAssembly.Table.prototype.length' accessor property
const lengthDesc = Object.getOwnPropertyDescriptor(tableProto, 'length');
assertEq(typeof lengthDesc.get, "function");
assertEq(lengthDesc.set, undefined);
assertEq(lengthDesc.enumerable, false);
assertEq(lengthDesc.configurable, true);

// 'WebAssembly.Table.prototype.length' getter
const lengthGetter = lengthDesc.get;
assertEq(lengthGetter.length, 0);
assertErrorMessage(() => lengthGetter.call(), TypeError, /called on incompatible undefined/);
assertErrorMessage(() => lengthGetter.call({}), TypeError, /called on incompatible Object/);
assertEq(typeof lengthGetter.call(tbl1), "number");
assertEq(lengthGetter.call(tbl1), 2);

// 'WebAssembly.Table.prototype.get' property
const getDesc = Object.getOwnPropertyDescriptor(tableProto, 'get');
assertEq(typeof getDesc.value, "function");
assertEq(getDesc.enumerable, false);
assertEq(getDesc.configurable, true);

// 'WebAssembly.Table.prototype.get' method
const get = getDesc.value;
assertEq(get.length, 1);
assertErrorMessage(() => get.call(), TypeError, /called on incompatible undefined/);
assertErrorMessage(() => get.call({}), TypeError, /called on incompatible Object/);
assertEq(get.call(tbl1, 0), null);
assertEq(get.call(tbl1, 1), null);
assertEq(get.call(tbl1, 1.5), null);
assertErrorMessage(() => get.call(tbl1, 2), RangeError, /out-of-range index/);
assertErrorMessage(() => get.call(tbl1, 2.5), RangeError, /out-of-range index/);
assertErrorMessage(() => get.call(tbl1, -1), RangeError, /out-of-range index/);
assertErrorMessage(() => get.call(tbl1, Math.pow(2,33)), RangeError, /out-of-range index/);
assertErrorMessage(() => get.call(tbl1, {valueOf() { throw new Error("hi") }}), Error, "hi");

// 'WebAssembly.Table.prototype.set' property
const setDesc = Object.getOwnPropertyDescriptor(tableProto, 'set');
assertEq(typeof setDesc.value, "function");
assertEq(setDesc.enumerable, false);
assertEq(setDesc.configurable, true);

// 'WebAssembly.Table.prototype.set' method
const set = setDesc.value;
assertEq(set.length, 2);
assertErrorMessage(() => set.call(), TypeError, /called on incompatible undefined/);
assertErrorMessage(() => set.call({}), TypeError, /called on incompatible Object/);
assertErrorMessage(() => set.call(tbl1, 0), TypeError, /requires more than 1 argument/);
assertErrorMessage(() => set.call(tbl1, 2, null), RangeError, /out-of-range index/);
assertErrorMessage(() => set.call(tbl1, -1, null), RangeError, /out-of-range index/);
assertErrorMessage(() => set.call(tbl1, Math.pow(2,33), null), RangeError, /out-of-range index/);
assertErrorMessage(() => set.call(tbl1, 0, undefined), TypeError, /second argument must be null or an exported WebAssembly Function object/);
assertErrorMessage(() => set.call(tbl1, 0, {}), TypeError, /second argument must be null or an exported WebAssembly Function object/);
assertErrorMessage(() => set.call(tbl1, 0, function() {}), TypeError, /second argument must be null or an exported WebAssembly Function object/);
assertErrorMessage(() => set.call(tbl1, 0, Math.sin), TypeError, /second argument must be null or an exported WebAssembly Function object/);
assertErrorMessage(() => set.call(tbl1, {valueOf() { throw Error("hai") }}, null), Error, "hai");
assertEq(set.call(tbl1, 0, null), undefined);
assertEq(set.call(tbl1, 1, null), undefined);

// 'WebAssembly.compile' property
const compileDesc = Object.getOwnPropertyDescriptor(WebAssembly, 'compile');
assertEq(typeof compileDesc.value, "function");
assertEq(compileDesc.writable, true);
assertEq(compileDesc.enumerable, false);
assertEq(compileDesc.configurable, true);

// 'WebAssembly.compile' function
const compile = WebAssembly.compile;
assertEq(compile, compileDesc.value);
assertEq(compile.length, 1);
assertEq(compile.name, "compile");
function assertCompileError(args, msg) {
    var error = null;
    compile(...args).catch(e => error = e);
    drainJobQueue();
    assertEq(error instanceof TypeError, true);
    assertEq(Boolean(error.stack.match("jsapi.js")), true);
    assertEq(Boolean(error.message.match(msg)), true);
}
assertCompileError([], /requires more than 0 arguments/);
assertCompileError([undefined], /first argument must be an ArrayBuffer or typed array object/);
assertCompileError([1], /first argument must be an ArrayBuffer or typed array object/);
assertCompileError([{}], /first argument must be an ArrayBuffer or typed array object/);
assertCompileError([new Uint8Array()], /compile error/);
assertCompileError([new ArrayBuffer()], /compile error/);
function assertCompileSuccess(bytes) {
    var module = null;
    compile(bytes).then(m => module = m);
    drainJobQueue();
    assertEq(module instanceof Module, true);
}
assertCompileSuccess(emptyModule);
assertCompileSuccess(emptyModule.buffer);
