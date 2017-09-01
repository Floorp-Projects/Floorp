// |jit-test| error: ReferenceError

let m = parseModule("export let r = y; y = 4;");
getModuleEnvironmentValue(m, "r").toString()
