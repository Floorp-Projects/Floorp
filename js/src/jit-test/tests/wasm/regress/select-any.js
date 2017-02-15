load(libdir + "wasm.js");

// Bug 1280921

var m1 = wasmEvalText(
`(module
  (type $type0 (func))
  (func $func0
   (select (unreachable) (return (nop)) (loop (i32.const 1))))
  (export "" 0))`).exports[""];

try {
    m1();
} catch (e) {
    if (!(e instanceof Error && e.message.match(/unreachable executed/)))
	throw e;
}

var m2 = wasmEvalText(
`(module
  (type $type0 (func))
  (func $func0
   (select (i32.const 26) (unreachable) (i32.const 3)))
  (export "" 0))`).exports[""];

try {
    m2();
} catch (e) {
    if (!(e instanceof Error && e.message.match(/unreachable executed/)))
	throw e;
}
