// We have some register args and the rest are on the stack in the wasm->js
// call.  This tests a case where the wrong frame offset was used (on ARM64,
// which is weird in this regard) to read the stack arg in an optimized callout
// stub.

var loop_counter = 0;

function f(a,b,c,d,e,f,g,h,i,j) {
    assertEq(a, loop_counter);
    assertEq(b, a+1);
    assertEq(c, b+1);
    assertEq(d, c+1);
    assertEq(e, d+1);
    assertEq(f, e+1);
    assertEq(g, f+1);
    assertEq(h, g+1);
    assertEq(i, h+1);
    assertEq(j, i+1);
}

var bin = wasmTextToBinary(
    `(module
       (import $f "m" "f" (func (param i32) (param i32) (param i32) (param i32) (param i32)
                                (param i32) (param i32) (param i32) (param i32) (param i32)))
       (func (export "test") (param $a i32) (param $b i32) (param $c i32) (param $d i32) (param $e i32)
                             (param $f i32) (param $g i32) (param $h i32) (param $i i32) (param $j i32)
         (call $f (local.get $a) (local.get $b) (local.get $c) (local.get $d) (local.get $e)
                  (local.get $f) (local.get $g) (local.get $h) (local.get $i) (local.get $j))))`);

var mod = new WebAssembly.Module(bin);
var ins = new WebAssembly.Instance(mod, {m:{f}});

// Enough iterations for the jit to kick in and the stub to become optimized.

for ( let i=0; i < 100; i++ ) {
    loop_counter = i;
    ins.exports.test(i, i+1, i+2, i+3, i+4, i+5, i+6, i+7, i+8, i+9);
}
