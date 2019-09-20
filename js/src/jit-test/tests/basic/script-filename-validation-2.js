load(libdir + "asserts.js");
load(libdir + 'bytecode-cache.js');

// Install the callback after evaluating the script and saving the bytecode
// (generation 0). XDR decoding after this should throw.

var g = newGlobal({cloneSingletons: true});
test = `
  assertEq(generation, 0);
`;
assertThrowsInstanceOf(() => {
  evalWithCache(test, {
    global: g,
    checkAfter: function (ctx) {
      assertEq(g.generation, 0);
      setTestFilenameValidationCallback();
    }
  });
}, g.InternalError);

// Generation should be 1 (XDR decoding threw an exception).
assertEq(g.generation, 1);
