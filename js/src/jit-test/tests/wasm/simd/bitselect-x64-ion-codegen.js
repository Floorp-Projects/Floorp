// |jit-test| skip-if: !wasmSimdEnabled() || !hasDisassembler() || wasmCompileMode() != "ion" || !getBuildConfiguration().x64 || getBuildConfiguration().simulator; include:codegen-x64-test.js

// Test that there are no extraneous moves or fixups for SIMD bitselect
// operations.  See README-codegen.md for general information about this type of
// test case.

// The codegen enforces onTrue == output so we avoid a move to set that up.
//
// The remaining movdqa is currently unavoidable, it moves the control mask into a temp.
// The temp should be identical to the mask but the regalloc does not currently
// allow this constraint to be enforced.

// Inputs (xmm0, xmm1, xmm2)

codegenTestX64_adhoc(
`(module
    (func (export "f") (param v128) (param v128) (param v128) (param v128) (result v128)
      (v128.bitselect (local.get 0) (local.get 1) (local.get 2))))`,
    'f',
`66 0f 6f da               movdqa %xmm2, %xmm3
66 0f db c3               pand %xmm3, %xmm0
66 0f df d9               pandn %xmm1, %xmm3
66 0f eb c3               por %xmm3, %xmm0`);

