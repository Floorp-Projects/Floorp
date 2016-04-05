// |jit-test| error: x is not defined

// enableShellAllocationMetadataBuilder ignores its argument, because we don't
// permit metadata callbacks to run JS any more, so this test may be
// unnecessary. We'll preserve its structure just in case.
enableShellAllocationMetadataBuilder(function(obj) {
    var res = {};
    return res;
  });
gczeal(4);
x();
