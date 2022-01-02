// |jit-test| skip-if: !('oomTest' in this)

oomTest(() => {
  let memory = new WebAssembly.Memory({initial: 0});
  assertEq(Object.getPrototypeOf(memory), WebAssembly.Memory.prototype, "prototype");
});

oomTest(() => {
  let global = new WebAssembly.Global({value: 'i32'});
  assertEq(Object.getPrototypeOf(global), WebAssembly.Global.prototype, "prototype");
});

oomTest(() => {
  let table = new WebAssembly.Table({element: 'anyfunc', initial: 0});
  assertEq(Object.getPrototypeOf(table), WebAssembly.Table.prototype, "prototype");
});

oomTest(() => {
  let bytecode = wasmTextToBinary('(module)');
  let module = new WebAssembly.Module(bytecode);
  assertEq(Object.getPrototypeOf(module), WebAssembly.Module.prototype, "prototype");
});

oomTest(() => {
  let bytecode = wasmTextToBinary('(module)');
  let module = new WebAssembly.Module(bytecode);
  let instance = new WebAssembly.Instance(module, {});
  assertEq(Object.getPrototypeOf(instance), WebAssembly.Instance.prototype, "prototype");
});
