// |jit-test| error: Error
let m = parseModule(`for (var x of iterator) {}`);
m.declarationInstantiation();
try { m.evaluation(); } catch (e) {}
getModuleEnvironmentValue(m, "r");
