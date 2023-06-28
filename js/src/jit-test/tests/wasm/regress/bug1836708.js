// Testing i64.mul in wasm with a 2 ** n +- 1 value as operand, which may
// be optimized in code generation.
var mulImmOperands = [];

for (let i = 0n; i < 64n; i++) {
  mulImmOperands.push(2n ** i - 1n);
  mulImmOperands.push(2n ** i + 1n);
}

for (const immVal of mulImmOperands) {
  const ins = wasmEvalText(`(module
    (func (export "mul_i64") (param i64) (result i64)
      local.get 0
      i64.const ${immVal}
      i64.mul
    ))`);

  assertEq(ins.exports.mul_i64(42n), BigInt.asIntN(64, 42n * immVal));
}
