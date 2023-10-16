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
