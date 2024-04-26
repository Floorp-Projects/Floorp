// Tests OOM during wasmLosslessInvoke.

var ins = wasmEvalText('(module (func (export "f")(result i32) i32.const 1))');

oomAtAllocation(1);
try {
  wasmLosslessInvoke(ins.exports.f);
} catch (e) {
  assertEq(e, "out of memory");
}
