// Test 'this' is undefined in modules.

function parseAndEvaluate(source) {
    let m = parseModule(source);
    instantiateModule(m);
    return evaluateModule(m);
}

assertEq(typeof(parseAndEvaluate("this")), "undefined");

let m = parseModule("export function getThis() { return this; }");
instantiateModule(m);
evaluateModule(m);
let f = getModuleEnvironmentValue(m, "getThis");
assertEq(typeof(f()), "undefined");
