// |jit-test| test-also=--wasm-exceptions; test-also=--wasm-function-references; test-also=--wasm-gc

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
  ['exceptions', wasmExceptionsEnabled(), `(module (type (func)) (event (type 0)))`],
  ['function-references', wasmFunctionReferencesEnabled(), `(module (func (param (ref extern))))`],
  ['gc', wasmGcEnabled(), `(module (type $s (struct)) (func (param (ref null $s))))`],
];

for (let [name, enabled, test] of nightlyOnlyFeatures) {
  if (enabled) {
    assertEq(nightly, true, `${name} must be enabled only on nightly`);
    wasmEvalText(test);
  } else {
    assertErrorMessage(() => wasmEvalText(test), WebAssembly.CompileError, /./);
  }
}

// These are features that are enabled in beta/release but may be disabled at
// run-time for other reasons.  The best we can do for these features is to say
// that if one claims to be supported then it must work, and otherwise there
// must be a CompileError.

let releasedFeaturesMaybeDisabledAnyway = [
  // SIMD will be disabled dynamically on x86/x64 if the hardware isn't SSE4.1+.
  ['simd', wasmSimdEnabled(), `(module (func (result v128) i32.const 0 i8x16.splat))`]
];

for (let [name, enabled, test] of releasedFeaturesMaybeDisabledAnyway) {
  if (release_or_beta) {
    if (enabled) {
      wasmEvalText(test);
    } else {
      assertErrorMessage(() => wasmEvalText(test), WebAssembly.CompileError, /./);
    }
  }
}

let releasedFeatures = [
  ['threads', wasmThreadsEnabled(), `(module (memory 1 1 shared))`],
];

for (let [name, enabled, test] of releasedFeatures) {
  if (release_or_beta) {
    assertEq(enabled, true, `${name} must be enabled on release and beta`);
    wasmEvalText(test);
  }
}
