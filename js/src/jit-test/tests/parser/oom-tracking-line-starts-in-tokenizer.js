// Constraints on this test's appearance:
//
//   * |TokenStream::SourceCoords::add| must try to allocate memory.  (This test
//     ensures this happens by making the function below >=128 lines long so
//     that |SourceCoords::lineStartOffsets_| must convert to heap storage.  The
//     precise approach doesn't matter.)
//   * That allocation attempt must fail (by forced simulated OOM, here).
//
// It'd be nice to build up the function programmatically, but it appears that
// the above only happens if the provided function has a lazy script.  Cursory
// attempts to relazify |Function("...")| didn't work, so this fuzzer-like
// version had to be used instead.
if ("oomTest" in this) {
  oomTest(function() {
      try {



























































































































      } catch(e) {
          ;
      }
  })
}
