// |jit-test| error: Error
let m = parseModule(`for (var x of iterator) {}`);
instantiateModule(m);
try { evaluateModule(m); } catch (e) {}
getModuleEnvironmentValue(m, "r");
