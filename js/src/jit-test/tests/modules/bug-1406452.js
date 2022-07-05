// |jit-test| error: Error
let m = parseModule(`for (var x of iterator) {}`);
moduleLink(m);
try { moduleEvaluate(m); } catch (e) {}
getModuleEnvironmentValue(m, "r");
