load(libdir + "asserts.js");
load(libdir + "dummyModuleResolveHook.js");

moduleRepo["a"] = parseModule(`throw undefined`);

let b = moduleRepo["b"] = parseModule(`import "a";`);
let c = moduleRepo["c"] = parseModule(`import "a";`);

b.declarationInstantiation();
c.declarationInstantiation();

let count = 0;
try { b.evaluation() } catch (e) { count++; }
try { c.evaluation() } catch (e) { count++; }
assertEq(count, 2);
