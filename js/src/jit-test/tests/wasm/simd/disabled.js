// |jit-test| skip-if: wasmSimdEnabled()

// ../binary.js checks that all SIMD extended opcodes in the 0..255 range are
// rejected if !wasmSimdEnabled, so no need to check that here.

// Non-opcode cases that should also be rejected, lest feature sniffing may
// erroneously conclude that simd is available when it's not.  The error message
// may differ depending on ENABLE_WASM_SIMD: if SIMD is compiled in we usually
// get a sensible error about v128; if not, we get something generic.

wasmFailValidateText(`(module (func (param v128)))`,
                     /(v128 not enabled)|(bad type)/);

wasmFailValidateText(`(module (func (result v128)))`,
                     /(v128 not enabled)|(bad type)/);

wasmFailValidateText(`(module (func (local v128)))`,
                     /(v128 not enabled)|(bad type)|(SIMD support is not enabled)/);

wasmFailValidateText(`(module (global (import "m" "g") v128))`,
                     /expected global type/);

wasmFailValidateText(`(module (global (import "m" "g") (mut v128)))`,
                     /expected global type/);

wasmFailValidateText(`(module (global i32 (v128.const i32x4 0 0 0 0)))`,
                     /(v128 not enabled)|(unexpected initializer opcode)/);

