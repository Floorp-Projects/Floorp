// |jit-test| skip-if: !hasDisassembler() || !(wasmCompileMode() == "baseline" || wasmCompileMode() == "ion") || !(getBuildConfiguration().x64 && !getBuildConfiguration()["arm64-simulator"] && !getBuildConfiguration()["mips64-simulator"])

// What we can't test here is the direct-call-from-JIT path, as the generated
// code is not available to wasmDis.

var ins = wasmEvalText(`
(module
  (import "" "wasm2import" (func $g (result i32)))
  (memory 1)
  (type $ty (func (result i32)))
  (table $t 1 1 funcref)
  (func $f (result i32)
    (i32.const 37))
  (func (export "wasm2wasm") (result i32)
    (call $f))
  (func (export "wasm2import") (result i32)
    (call $g))
  (func (export "wasmIndirect") (result i32)
    (call_indirect (type $ty) $t (i32.const 0)))
  (func (export "instanceCall") (result i32)
    (memory.size))
)`, {'':{'wasm2import': function() {}}});

switch (wasmCompileMode()) {
case "ion":
    assertEq(wasmDis(ins.exports.wasm2wasm, 'stable', true).match(/call.*\n.*mov %eax, %eax/).length, 1);
    assertEq(wasmDis(ins.exports.wasm2import, 'stable', true).match(/call.*\n(?:.*movq.*\n)*.*mov %eax, %eax/).length, 1);
    assertEq(wasmDis(ins.exports.wasmIndirect, 'stable', true).match(/call.*\n(?:.*movq.*\n)*.*mov %eax, %eax/).length, 1);
    assertEq(wasmDis(ins.exports.instanceCall, 'stable', true).match(/call.*\n(?:.*movq.*\n)*.*mov %eax, %eax/).length, 1);
    break;
case "baseline":
    assertEq(wasmDis(ins.exports.wasm2wasm, 'stable', true).match(/call.*\n.*add.*%rsp\n.*mov %eax, %eax/).length, 1);
    assertEq(wasmDis(ins.exports.wasm2import, 'stable', true).match(/call.*\n.*add.*%rsp\n(?:.*movq.*\n)*.*mov %eax, %eax/).length, 1);
    assertEq(wasmDis(ins.exports.wasmIndirect, 'stable', true).match(/call.*\n.*add.*%rsp\n(?:.*movq.*\n)*.*mov %eax, %eax/).length, 1);
    assertEq(wasmDis(ins.exports.instanceCall, 'stable', true).match(/call.*\n.*add.*%rsp\n(?:.*movq.*\n)*.*mov %eax, %eax/).length, 1);
    break;
default:
    throw "Unexpected compile mode";
}

