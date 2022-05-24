// |jit-test| --wasm-compiler=optimizing; --wasm-simd-wormhole; skip-if: !wasmSimdWormholeEnabled() || !hasDisassembler() || !getBuildConfiguration().x64 || wasmCompileMode() != "ion"; include:codegen-x64-test.js; include:wasm-binary.js

function wormhole_op(opcode) {
    return `i8x16.shuffle 31 0 30 2 29 4 28 6 27 8 26 10 25 12 24 ${opcode} `
}

var instanceBox = {value:null};

codegenTestX64_adhoc(
`(module
   (memory (export "mem") 1)
   (func (export "run")
     (v128.store (i32.const 0) (call $test (v128.load (i32.const 16)) (v128.load (i32.const 32)))))
   (func $test (export "PMADDUBSW") (param v128) (param v128) (result v128)
     (${wormhole_op(WORMHOLE_PMADDUBSW)} (local.get 0) (local.get 1))))`,
    'PMADDUBSW',
    '66 0f 38 04 c1            pmaddubsw %xmm1, %xmm0',
    {instanceBox, features:{simdWormhole:true}});

var ins = instanceBox.value;
var mem8 = new Uint8Array(ins.exports.mem.buffer);
var a = iota(16).map(x => x+2);
var b = iota(16).map(x => x+3);
set(mem8, 16, a);
set(mem8, 32, b);
ins.exports.run();
var mem16 = new Int16Array(ins.exports.mem.buffer);
var res = get(mem16, 0, 8);
var prods = iota(16).map(i => a[i] * b[i]);
var sums = iota(8).map(i => prods[i*2] + prods[i*2+1]);
assertSame(sums, res);

codegenTestX64_adhoc(
`(module
   (memory (export "mem") 1)
   (func (export "run")
     (v128.store (i32.const 0) (call $test (v128.load (i32.const 16)) (v128.load (i32.const 32)))))
   (func $test (export "PMADDWD") (param v128) (param v128) (result v128)
     (${wormhole_op(WORMHOLE_PMADDWD)} (local.get 0) (local.get 1))))`,
    'PMADDWD',
    '66 0f f5 c1               pmaddwd %xmm1, %xmm0',
    {instanceBox, features:{simdWormhole:true}});

var ins = instanceBox.value;
var mem16 = new Int16Array(ins.exports.mem.buffer);
var a = iota(8).map(x => x+2);
var b = iota(8).map(x => x+3);
set(mem16, 8, a);
set(mem16, 16, b);
ins.exports.run();
var mem32 = new Int32Array(ins.exports.mem.buffer);
var res = get(mem32, 0, 4);
var prods = iota(8).map(i => a[i] * b[i]);
var sums = iota(4).map(i => prods[i*2] + prods[i*2+1]);
assertSame(sums, res);

// Library

function get(arr, loc, len) {
    return iota(len).map(i => arr[loc+i]);
}

function set(arr, loc, vals) {
    iota(vals.length).map(i => arr[loc+i] = vals[i]);
}
