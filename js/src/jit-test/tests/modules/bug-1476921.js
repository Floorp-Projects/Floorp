"use strict";

load(libdir + "asserts.js");
load(libdir + "dummyModuleResolveHook.js");

class UniqueError extends Error {}

let a = moduleRepo['a'] = parseModule(`
    throw new UniqueError();
`);

let b = moduleRepo['b'] = parseModule(`
    import * as ns0 from "a";
`);

a.declarationInstantiation();
assertThrowsInstanceOf(() => a.evaluation(), UniqueError);
b.declarationInstantiation();
assertThrowsInstanceOf(() => b.evaluation(), UniqueError);
