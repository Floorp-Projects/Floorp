// |jit-test| include:wasm.js

let {test} = wasmEvalText(`(module
	(func $m (import "" "m"))
	(func (export "test")
	  call $m
	)
)`, {"": {m: () => {throw 'wrap me';}}}).exports;

try {
  test();
} catch (err) {
	assertEq(err, 'wrap me');
}
