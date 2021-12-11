class _Module extends WebAssembly.Module {}
class _Instance extends WebAssembly.Instance {}
class _Memory extends WebAssembly.Memory {}
class _Table extends WebAssembly.Table {}
class _Global extends WebAssembly.Global {}

let binary = wasmTextToBinary('(module)');

let module = new _Module(binary);
assertEq(module instanceof _Module, true);
assertEq(module instanceof WebAssembly.Module, true);

let instance = new _Instance(module);
assertEq(instance instanceof _Instance, true);
assertEq(instance instanceof WebAssembly.Instance, true);

let memory = new _Memory({initial: 0, maximum: 1});
assertEq(memory instanceof _Memory, true);
assertEq(memory instanceof WebAssembly.Memory, true);

let table = new _Table({initial: 0, element: 'anyfunc'});
assertEq(table instanceof _Table, true);
assertEq(table instanceof WebAssembly.Table, true);

let global = new _Global({value: 'i32', mutable: false}, 0);
assertEq(global instanceof _Global, true);
assertEq(global instanceof WebAssembly.Global, true);
