load(libdir + "dummyModuleResolveHook.js");

let a = moduleRepo['a'] = parseModule(`
  export var { ... get } = { x: "foo" };
`);

let m = parseModule("import { get } from 'a'; export { get };");
m.declarationInstantiation();
m.evaluation()
assertEq(getModuleEnvironmentValue(m, "get").x, "foo");
