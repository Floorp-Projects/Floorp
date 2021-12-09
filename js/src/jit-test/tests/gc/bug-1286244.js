// |jit-test| skip-if: !getBuildConfiguration()['has-gczeal'] || helperThreadCount() === 0

// This will fail with --no-threads.
verifyprebarriers();
var lfGlobal = newGlobal();
lfGlobal.offThreadCompileScript(`
  version(185);
`);
