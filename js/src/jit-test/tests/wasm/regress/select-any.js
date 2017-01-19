load(libdir + "wasm.js");

// Bug 1280921

wasmFailValidateText(
`(module
  (type $type0 (func))
  (func $func0
   (select (unreachable) (return (nop)) (loop (i32.const 1))))
  (export "" 0))`, /non-fallthrough instruction must be followed by end or else/);

wasmFailValidateText(
`(module
  (type $type0 (func))
  (func $func0
   (select (i32.const 26) (unreachable) (i32.const 3)))
  (export "" 0))`, /non-fallthrough instruction must be followed by end or else/);

var m3 = wasmEvalText(
`(module
  (type $type0 (func))
  (func $func0
   (select (block i32 (unreachable)) (block i32 (return (nop))) (loop (i32.const 1))))
  (export "" 0))`).exports[""];

try {
    m3();
} catch (e) {
    if (!(e instanceof Error && e.message.match(/unreachable executed/)))
	throw e;
}

var m4 = wasmEvalText(
`(module
  (type $type0 (func))
  (func $func0
   (select (i32.const 26) (block i32 (unreachable)) (i32.const 3)))
  (export "" 0))`).exports[""];

try {
    m4();
} catch (e) {
    if (!(e instanceof Error && e.message.match(/unreachable executed/)))
	throw e;
}
