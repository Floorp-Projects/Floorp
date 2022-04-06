var ins = wasmEvalText(`
(module
  (func (export "copysign_f64") (param f64 f64) (result f64)
    f64.const 0x1.921fb54442d18p+0 (;=1.5708;)
    local.get 0
    f64.copysign
  )
  (func (export "copysign_f32") (param f32 f32) (result f32)
    f32.const 0x1.921fb54442d18p+0 (;=1.5708;)
    local.get 0
    f32.copysign
  )
)
`);

assertEq(ins.exports.copysign_f64(1, 0), 1.5707963267948966);
assertEq(ins.exports.copysign_f64(-1, 0), -1.5707963267948966);
assertEq(ins.exports.copysign_f32(1, 0), 1.5707963705062866);
assertEq(ins.exports.copysign_f32(-1, 0), -1.5707963705062866);
