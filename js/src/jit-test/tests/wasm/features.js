// |jit-test| test-also=--wasm-gc;

// Test that if a feature is 'experimental' then we must be in a nightly build,
// and if a feature is 'released' then it must be enabled on release and beta.
//
// An experimental feature is allowed to be disabled by platform/feature flags,
// we only require that it can never be enabled on release/beta builds.
//
// A released feature is allowed to be disabled on nightly. This is useful for
// if we're testing out a new compiler/configuration where a released feature
// is not supported yet. We only require that on release/beta, the feature must
// be enabled.
//
// As features are advanced, this test must be manually updated.
//
// NOTE0: The |jit-test| directive must be updated with all opt-in shell flags
//        for experimental features for this to work correctly.
// NOTE1: This test relies on feature functions accurately guarding use of the
//        feature to work correctly. All features should have a 'disabled.js'
//        test to verify this. Basic testing for this is included with each
//        feature in this test for sanity.

let { release_or_beta } = getBuildConfiguration();
let nightly = !release_or_beta;

let nightlyOnlyFeatures = [
  ['gc', wasmGcEnabled(), `(module (type $s (struct)) (func (param (ref null $s))))`],
  ['simd', wasmSimdSupported(), `(module (memory 1 1) (func i32.const 0 i8x16.splat drop))`],
];

for (let [name, enabled, test] of nightlyOnlyFeatures) {
  if (enabled) {
    assertEq(nightly, true, `${name} must be enabled only on nightly`);
    wasmEvalText(test);
  } else {
    assertErrorMessage(() => wasmEvalText(test), WebAssembly.CompileError, /./);
  }
}

let releasedFeatures = [
  ['multi-value', wasmMultiValueEnabled(), `(module (func (result i32 i32) i32.const 0 i32.const 0))`],
  ['threads', wasmThreadsSupported(), `(module (memory 1 1 shared))`],
  ['reference-types', wasmReftypesEnabled(), `(module (func (param externref)))`],
];

for (let [name, enabled, test] of releasedFeatures) {
  if (release_or_beta) {
    assertEq(enabled, true, `${name} must be enabled on release and beta`);
    wasmEvalText(test);
  }
}
