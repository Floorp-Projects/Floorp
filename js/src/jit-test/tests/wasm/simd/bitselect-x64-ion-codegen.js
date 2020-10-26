// |jit-test| skip-if: !wasmSimdEnabled() || wasmCompileMode() != "ion" || !getBuildConfiguration().x64

// Test that there are no extraneous moves or fixups for SIMD bitselect
// operations.  See README-codegen.md for general information about this type of
// test case.

// The codegen enforces onTrue == output so we avoid a move to set that up.
//
// The remaining movdqa is currently unavoidable, it moves the control mask into a temp.
// The temp should be identical to the mask but the regalloc does not currently
// allow this constraint to be enforced.

let expected = `
000000..  48 8b ec                  mov %rsp, %rbp
000000..  66 0f 6f da               movdqa %xmm2, %xmm3
000000..  66 0f db c3               pand %xmm3, %xmm0
000000..  66 0f df d9               pandn %xmm1, %xmm3
000000..  66 0f eb c3               por %xmm3, %xmm0
000000..  5d                        pop %rbp
`;

let ins = wasmEvalText(`
  (module
    (func (export "f") (param v128) (param v128) (param v128) (param v128) (result v128)
      (v128.bitselect (local.get 0) (local.get 1) (local.get 2))))
        `);
let output = wasmDis(ins.exports.f, "ion", true);
if (output.indexOf('No disassembly available') < 0) {
    assertEq(output.match(new RegExp(expected)) != null, true);
}
