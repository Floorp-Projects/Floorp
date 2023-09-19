// |jit-test| test-also=--wasm-compiler=optimizing; skip-if: !wasmDebuggingEnabled()
// Checking if we destroying work registers by breakpoint/step handler.

load(libdir + "wasm.js");

// Running the following code compiled from C snippet:
//
//     signed func0(signed n) {
//         double a = 1; float b = 0; signed c = 1; long long d = 1;
//         for (;n > 0; n--) {
//             a *= c; b += c; c++; d <<= 1;
//         }
//         return (signed)a + (signed)b + c + (signed)d;
//     }
//
var onStepCalled;
wasmRunWithDebugger(
  `(module
  (func $func0 (param $var0 i32) (result i32)
    (local $var1 i32) (local $var2 i64) (local $var3 f64) (local $var4 f32)
    (local $var5 f64) (local $var6 i32) (local $var7 i32) (local $var8 i32)
    i32.const 1
    local.set $var1
    i32.const 0
    local.set $var7
    i32.const 1
    local.set $var6
    i32.const 1
    local.set $var8
    block $label0
      local.get $var0
      i32.const 1
      i32.lt_s
      br_if $label0
      local.get $var0
      i32.const 1
      i32.add
      local.set $var1
      f32.const 0
      local.set $var4
      i64.const 1
      local.set $var2
      f64.const 1
      local.set $var3
      i32.const 1
      local.set $var0
      f64.const 1
      local.set $var5
      block
        loop $label1
          local.get $var2
          i64.const 1
          i64.shl
          local.set $var2
          local.get $var5
          local.get $var3
          f64.mul
          local.set $var5
          local.get $var4
          local.get $var0
          f32.convert_i32_s
          f32.add
          local.set $var4
          local.get $var3
          f64.const 1
          f64.add
          local.set $var3
          local.get $var0
          i32.const 1
          i32.add
          local.tee $var6
          local.set $var0
          local.get $var1
          i32.const -1
          i32.add
          local.tee $var1
          i32.const 1
          i32.gt_s
          br_if $label1
        end $label1
      end
      local.get $var2
      i32.wrap_i64
      local.set $var1
      local.get $var4
      i32.trunc_f32_s
      local.set $var7
      local.get $var5
      i32.trunc_f64_s
      local.set $var8
    end $label0
    local.get $var7
    local.get $var8
    i32.add
    local.get $var6
    i32.add
    local.get $var1
    i32.add
    return
  )
  (func (export "test") (result i32) (call $func0 (i32.const 4)))
)`.split('\n').join(' '),
    undefined,
    function ({dbg}) {
        onStepCalled = [];
        dbg.onEnterFrame = function (frame) {
            if (frame.type != 'wasmcall') return;
            frame.onStep = function () {
                onStepCalled.push(frame.offset);
            };
        };
  },
  function ({result, error}) {
      assertEq(result, /* func0(4) = */ 55);
      assertEq(error, undefined);
      // The number below reflects amount of wasm operators executed during
      // the run of the test function, which runs $func0(4). It can change
      // when the code above and/or meaning of wasm operators will change.
      assertEq(onStepCalled.length, 85);
  }
);
