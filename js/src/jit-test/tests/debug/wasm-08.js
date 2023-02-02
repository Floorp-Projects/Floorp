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
    set_local $var1
    i32.const 0
    set_local $var7
    i32.const 1
    set_local $var6
    i32.const 1
    set_local $var8
    block $label0
      get_local $var0
      i32.const 1
      i32.lt_s
      br_if $label0
      get_local $var0
      i32.const 1
      i32.add
      set_local $var1
      f32.const 0
      set_local $var4
      i64.const 1
      set_local $var2
      f64.const 1
      set_local $var3
      i32.const 1
      set_local $var0
      f64.const 1
      set_local $var5
      block
        loop $label1
          get_local $var2
          i64.const 1
          i64.shl
          set_local $var2
          get_local $var5
          get_local $var3
          f64.mul
          set_local $var5
          get_local $var4
          get_local $var0
          f32.convert_s/i32
          f32.add
          set_local $var4
          get_local $var3
          f64.const 1
          f64.add
          set_local $var3
          get_local $var0
          i32.const 1
          i32.add
          tee_local $var6
          set_local $var0
          get_local $var1
          i32.const -1
          i32.add
          tee_local $var1
          i32.const 1
          i32.gt_s
          br_if $label1
        end $label1
      end
      get_local $var2
      i32.wrap/i64
      set_local $var1
      get_local $var4
      i32.trunc_s/f32
      set_local $var7
      get_local $var5
      i32.trunc_s/f64
      set_local $var8
    end $label0
    get_local $var7
    get_local $var8
    i32.add
    get_local $var6
    i32.add
    get_local $var1
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
