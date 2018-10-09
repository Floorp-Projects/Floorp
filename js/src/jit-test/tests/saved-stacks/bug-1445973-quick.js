// |jit-test| --no-baseline; skip-if: !('oomTest' in this)
//
// For background, see the comments for LiveSavedFrameCache in js/src/vm/Stack.h.
//
// The cache would like to assert that, assuming the cache hasn't been
// completely flushed due to a compartment mismatch, if a stack frame's
// hasCachedSavedFrame bit is set, then that frame does indeed have an entry in
// the cache.
//
// When LiveSavedFrameCache::find finds an entry whose frame address matches,
// but whose pc does not match, it removes that entry from the cache. Usually, a
// fresh entry for that frame will be pushed into the cache in short order as we
// rebuild the SavedFrame chain, but if the creation of the SavedFrame fails due
// to OOM, then we are left with no cache entry for that frame.
//
// The fix for 1445973 is simply to clear the frame's bit when we remove the
// cache entry for a pc mismatch. Previously the code neglected to do this, but
// usually got away with it because the cache would be re-populated. OOM fuzzing
// interrupted the code at the proper place and revealed the crash, but did so
// with a test that took 90s to run. This test runs in a fraction of a second.

function f() {
  // Ensure that we will try to allocate fresh SavedFrame objects.
  clearSavedFrames();

  // Ensure that all frames have their hasCachedSavedFrame bits set.
  saveStack();

  try {
    // Capture the stack again. The entry for this frame will be removed due to
    // a pc mismatch. The OOM must occur here, causing the cache not to be
    // repopulated.
    saveStack();
  } catch (e) { }

  // Capture the stack a third time. This will see that f's frame has its bit
  // set, even though it has no entry in the cache.
  saveStack();
}

// This ensures that there is a frame below f's in the same Activation, so that
// the assertion doesn't get skipped because the LiveSavedFrameCache is entirely
// empty, to handle caches flushed by compartment mismatches.
function g() { f(); }

// Call all the code once, to ensure that everything has been delazified. When
// different calls to g perform different amounts of allocation, oomTest's
// simple strategy for choosing which allocation should fail can neglect to hit
// the SavedFrame creation. This is also why we disable the baseline compiler in
// the test metaline.
g();

oomTest(g);
