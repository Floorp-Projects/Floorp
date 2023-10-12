// |jit-test| skip-if: getBuildConfiguration('pbl')
// (justification: PBL does not invoke the decompiler in the same way and so
// will not have an error message referring to the specific value name)
load(libdir + "asserts.js");

let foo = {};
for (let method of ["resolve", "reject", "race"]) {
  assertErrorMessage(
    () => Promise[method].call(foo),
    TypeError,
    "foo is not a constructor"
  );
  assertErrorMessage(
    () => Promise[method].call(foo, []),
    TypeError,
    "foo is not a constructor"
  );
  assertErrorMessage(
    () => Promise[method].call({}, [], foo),
    TypeError,
    "({}) is not a constructor"
  );
}
