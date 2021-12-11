// |jit-test|
"use strict";

load(libdir + "asserts.js");

class UniqueError extends Error {}

let a = registerModule('a', parseModule(`
    throw new UniqueError();
`));

let b = registerModule('b', parseModule(`
    import * as ns0 from "a";
`));

a.declarationInstantiation();
a.evaluation()
  .then(r => {
    // We should not reach here, as we expect an error to be thrown.
    assertEq(false, true);
  })
  .catch(e => assertEq(e instanceof UniqueError, true));
b.declarationInstantiation();
b.evaluation()
  .then(r => {
    // We should not reach here, as we expect an error to be thrown.
    assertEq(false, true);
  })
  .catch(e => assertEq(e instanceof UniqueError, true));

drainJobQueue();
