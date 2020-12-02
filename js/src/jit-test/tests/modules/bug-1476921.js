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
assertThrowsInstanceOf(() => a.evaluation(), UniqueError);
b.declarationInstantiation();
assertThrowsInstanceOf(() => b.evaluation(), UniqueError);
