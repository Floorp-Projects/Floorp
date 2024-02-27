// |jit-test| skip-if: !('wasmLosslessInvoke' in this)

let bytecode = wasmTextToBinary(`(module
	(func (export "f") (result i32)
		i32.const 1
	)
)`);
let g = newGlobal({sameCompartmentAs: wasmLosslessInvoke});
let m = new g.WebAssembly.Module(bytecode);
let i = new g.WebAssembly.Instance(m);

assertEq(i.exports.f(), 1);
assertEq(wasmLosslessInvoke(i.exports.f).value, 1);
