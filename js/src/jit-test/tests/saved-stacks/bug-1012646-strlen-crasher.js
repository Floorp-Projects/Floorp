// |jit-test| exitstatus: 3

enableTrackAllocations();
evaluate("throw Error();", {fileName: null});
