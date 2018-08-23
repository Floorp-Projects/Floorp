load(libdir + "asserts.js");
load(libdir + "dummyModuleResolveHook.js");

moduleRepo["a"] = parseModule(`throw undefined`);

let b = moduleRepo["b"] = parseModule(`import "a";`);
let c = moduleRepo["c"] = parseModule(`import "a";`);

instantiateModule(b);
instantiateModule(c);

let count = 0;
try { evaluateModule(b) } catch (e) { count++; }
try { evaluateModule(c) } catch (e) { count++; }
assertEq(count, 2);
