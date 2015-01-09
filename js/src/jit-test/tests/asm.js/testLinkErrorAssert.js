// |jit-test| test-also-noasmjs
// This test should not assert.

function asmModule(g, foreign, heap) {
    "use asm";
    let HEAP8 = new g.Int8Array(heap);

    function f() { return 99; } 
    return {f: f};
}

// linking error
let m = asmModule(this, 2, new ArrayBuffer(4095));

print(m.f());

// linking error
let m2 = asmModule(this, 2, new ArrayBuffer(2048));

print(m2.f());

