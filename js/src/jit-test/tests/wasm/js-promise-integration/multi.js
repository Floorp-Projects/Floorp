// Multiple promises at the same time.

function js_import() {
    return Promise.resolve(42);
}
var wasm_js_import = new WebAssembly.Function(
    { parameters: ['externref'], results: ['i32'] },
    js_import,
    { suspending: 'first' });

var ins = wasmEvalText(`(module
    (import "m" "import" (func $f (param externref) (result i32)))
    (func (export "test") (param externref) (result i32)
      local.get 0
      call $f
    )
)`, {"m": {import: wasm_js_import}});

let wrapped_export = new WebAssembly.Function(
    {
        parameters: [],
        results: ['externref']
    },
    ins.exports.test, { promising: 'first' });

Promise.resolve().then(() => {
    wrapped_export().then(i => {
        assertEq(42, i)
    });
});

Promise.resolve().then(() => {
    wrapped_export().then(i => {
        assertEq(42, i)
    });
});
