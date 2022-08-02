// |jit-test| slow; --ion-offthread-compile=off;
//
// Bug 1683309: Assertion failure: [barrier verifier] Unmarked edge: JS Object 0xebbb6d1dee0 'object slot' edge to JS Object 0xebbb6d29f60, at gc/Verifier.cpp:392


if (helperThreadCount() > 0) {
  evalInWorker(`
  try{
    gczeal(4);
    function f86(depth) {
      var x = async target => ([]);
      o62 = unescape;
      x(o62.prop, o62);
      f86(true + 1);
    }
    f86(0);
  } catch (e) {}
  `);
}
