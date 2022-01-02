// Moving a JS value through a wasm externref is a pair of boxing/unboxing
// conversions that leaves the value unchanged.  There are many cases,
// along these axes:
//
//  - global variables
//  - tables
//  - function parameters and returns
//  - struct fields [for the gc feature], see externref-boxing-struct.js

// Global variables can receive values via several mechanisms:
//
// - on initialization when created from JS
// - on initialization when created in Wasm, from an imported global
// - through the "value" property if the value is mutable
// - through the global.set wasm instruction, ditto
//
// Their values can be obtained in several ways:
//
// - through the "value" property
// - through the global.get wasm instruction
// - read when other globals are initialized from them

// Set via initialization and read via 'value'

for (let v of WasmExternrefValues)
{
    let g = new WebAssembly.Global({value: "externref"}, v);
    assertEq(g.value, v);
}

// Set via 'value' and read via 'value'

for (let v of WasmExternrefValues)
{
    let g = new WebAssembly.Global({value: "externref", mutable: true});
    g.value = v;
    assertEq(g.value, v);
}

// Set via initialization, then read via global.get and returned

for (let v of WasmExternrefValues)
{
    let g = new WebAssembly.Global({value: "externref"}, v);
    let ins = wasmEvalText(
        `(module
           (import "m" "g" (global $glob externref))
           (func (export "f") (result externref)
             (global.get $glob)))`,
        {m:{g}});
    assertEq(ins.exports.f(), v);
}

// Set via global.set, then read via 'value'

for (let v of WasmExternrefValues)
{
    let g = new WebAssembly.Global({value: "externref", mutable: true});
    let ins = wasmEvalText(
        `(module
           (import "m" "g" (global $glob (mut externref)))
           (func (export "f") (param $v externref)
             (global.set $glob (local.get $v))))`,
        {m:{g}});
    ins.exports.f(v);
    assertEq(g.value, v);
}

// Tables of externref can receive values via several mechanisms:
//
// - through WebAssembly.Table.prototype.set()
// - through the table.set, table.copy, and table.grow instructions
// - through table.fill
// - through WebAssembly.Table.prototype.grow()
//
// Their values can be read in several ways:
//
// - through WebAssembly.Table.prototype.get()
// - through the table.get and table.copy instructions
//
// (Note, tables are always initialized to null when created from JS)

// .set() and .get()

for (let v of WasmExternrefValues)
{
    let t = new WebAssembly.Table({element: "externref", initial: 10});
    t.set(3, v);
    assertEq(t.get(3), v);
}

// write with table.set, read with .get()

for (let v of WasmExternrefValues)
{
    let t = new WebAssembly.Table({element: "externref", initial: 10});
    let ins = wasmEvalText(
        `(module
           (import "m" "t" (table $t 10 externref))
           (func (export "f") (param $v externref)
             (table.set $t (i32.const 3) (local.get $v))))`,
        {m:{t}});
    ins.exports.f(v);
    assertEq(t.get(3), v);
}

// write with .set(), read with table.get

for (let v of WasmExternrefValues)
{
    let t = new WebAssembly.Table({element: "externref", initial: 10});
    let ins = wasmEvalText(
        `(module
           (import "m" "t" (table $t 10 externref))
           (func (export "f") (result externref)
             (table.get $t (i32.const 3))))`,
        {m:{t}});
    t.set(3, v);
    assertEq(ins.exports.f(), v);
}

// Imported JS functions can receive externref values as parameters and return
// them.

for (let v of WasmExternrefValues)
{
    let returner = function () { return v; };
    let receiver = function (w) { assertEq(w, v); };
    let ins = wasmEvalText(
        `(module
           (import "m" "returner" (func $returner (result externref)))
           (import "m" "receiver" (func $receiver (param externref)))
           (func (export "test_returner") (result externref)
             (call $returner))
           (func (export "test_receiver") (param $v externref)
             (call $receiver (local.get $v))))`,
        {m:{returner, receiver}});
    assertEq(ins.exports.test_returner(), v);
    ins.exports.test_receiver(v);
}
