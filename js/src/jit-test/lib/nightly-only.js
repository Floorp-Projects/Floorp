// Some experimental features are enabled only on nightly builds, and disabled
// on beta and release. Tests for these features should not simply disable
// themselves on all but nightly builds, because if we neglect to update such
// tests once the features cease to be experimental, we'll silently skip the
// tests on beta and release, even though they should run.

// Call the function f. On beta and release, expect it to throw an error that is
// an instance of error.
function nightlyOnly(error, f) {
  if (getBuildConfiguration().release_or_beta) {
    try {
      f();
      throw new Error("use of feature expected to fail on release and beta, but succeeded; please update test");
    } catch (e if e instanceof error) {
      // All is well.
    }
  } else {
    f();
  }
}
