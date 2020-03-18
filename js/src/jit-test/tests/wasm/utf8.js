/*
(module
 (func (param i32) (result i32)
       (i32.add (local.get 0) (local.get 0)))
 (export "hello" (func 0)))
*/

var txt =
`00 61 73 6d 01 00 00 00 01 06 01 60 01 7f 01 7f
 03 02 01 00 07 09 01 05 68 65 6c 6c 6f 00 00 0a
 09 01 07 00 20 00 20 00 6a 0b`

var bin = new Uint8Array(txt.split(" ").map((x) => parseInt(x, 16)));
var mod = new WebAssembly.Module(bin);
var ins = new WebAssembly.Instance(mod);

assertEq(ins.exports.hello(37), 2*37);

assertEq(bin[25], 0x65);	// the 'e' in 'hello'
bin[25] = 0x85;			// bad UTF-8

assertEq(WebAssembly.validate(bin), false);

assertThrowsInstanceOf(() => new WebAssembly.Module(bin), WebAssembly.CompileError);

assertThrowsInstanceOf(() => wasmEvalText(`(module (import "\u2603" "")) `, {}), TypeError);

{
    let i1 = wasmEvalText(` (module (func (export "\u2603")))`);
    assertThrowsInstanceOf(() => wasmEvalText(`(module (import "" "\u2603" (result i32)))`,
                                              { "": { "\u2603": i1.exports['\u2603'] } }),
                           WebAssembly.LinkError);
    assertThrowsInstanceOf(() => wasmEvalText(`(module (import "\u2603" "" (result i32)))`,
                                              { "\u2603": { "": i1.exports['\u2603'] } }),
                           WebAssembly.LinkError);
}
