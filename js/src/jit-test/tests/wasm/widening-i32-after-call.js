// |jit-test| skip-if: !hasDisassembler() || !(wasmCompileMode() == "baseline" || wasmCompileMode() == "ion") || !(getBuildConfiguration("x64") && !getBuildConfiguration("arm64-simulator") && !getBuildConfiguration("mips64-simulator"))

// We widen i32 results after calls on 64-bit platforms for two reasons:
//
// - it's a cheap mitigation for certain spectre problems, and
// - it makes the high bits of the 64-bit register conform to platform
//   conventions, which they might not if the call was to C++ code
//   especially.
//
// This is a whitebox test that explicit widening instructions are inserted
// after calls on x64.  The widening is platform-specific; on x64, the upper
// bits are zeroed.

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
    (call_indirect $t (type $ty) (i32.const 0)))
  (func (export "instanceCall") (result i32)
    (memory.size))
)`, {'':{'wasm2import': function() {}}});

switch (wasmCompileMode()) {
  case "ion":
    assertEq(wasmDis(ins.exports.wasm2wasm, {tier:'stable', asString:true}).match(/call.*\n(?:.*lea.*%rsp\n)?.*mov %eax, %eax/).length, 1);
    assertEq(wasmDis(ins.exports.wasm2import, {tier:'stable', asString:true}).match(/call.*\n(?:.*or \$0x00, %r14\n)?(?:.*movq.*\n)*(?:.*lea.*%rsp\n)?.*mov %eax, %eax/).length, 1);
    assertEq(wasmDis(ins.exports.wasmIndirect, {tier:'stable', asString:true}).match(/call.*\n(?:.*movq.*\n)*(?:.*lea.*%rsp\n)?.*mov %eax, %eax/).length, 1);
    assertEq(wasmDis(ins.exports.instanceCall, {tier:'stable', asString:true}).match(/call.*\n(?:.*movq.*\n)*.*mov %eax, %eax/).length, 1);
    break;
  case "baseline":
    assertEq(wasmDis(ins.exports.wasm2wasm, {tier:'stable', asString:true}).match(/call.*\n.*lea.*%rsp\n.*mov %eax, %eax/).length, 1);
    assertEq(wasmDis(ins.exports.wasm2import, {tier:'stable', asString:true}).match(/call.*\n(?:.*or \$0x00, %r14\n)?.*lea.*%rsp\n(?:.*movq.*\n)*.*mov %eax, %eax/).length, 1);
    assertEq(wasmDis(ins.exports.wasmIndirect, {tier:'stable', asString:true}).match(/call.*\n.*lea.*%rsp\n(?:.*movq.*\n)*.*mov %eax, %eax/).length, 1);
    assertEq(wasmDis(ins.exports.instanceCall, {tier:'stable', asString:true}).match(/call.*\n.*lea.*%rsp\n(?:.*movq.*\n)*.*mov %eax, %eax/).length, 1);
    break;
  default:
    throw "Unexpected compile mode";
}
