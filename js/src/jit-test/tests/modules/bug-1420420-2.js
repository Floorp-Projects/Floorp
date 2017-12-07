// Test re-instantiation module after failure.

load(libdir + "asserts.js");
load(libdir + "dummyModuleResolveHook.js");

moduleRepo["good"] = parseModule(`export let x`);

moduleRepo["y1"] = parseModule(`export let y`);
moduleRepo["y2"] = parseModule(`export let y`);
moduleRepo["bad"] = parseModule(`export* from "y1"; export* from "y2";`);

moduleRepo["a"] = parseModule(`import* as ns from "good"; import {y} from "bad";`);

let b = moduleRepo["b"] = parseModule(`import "a";`);
let c = moduleRepo["c"] = parseModule(`import "a";`);

assertThrowsInstanceOf(() => b.declarationInstantiation(), SyntaxError);
assertThrowsInstanceOf(() => c.declarationInstantiation(), SyntaxError);
