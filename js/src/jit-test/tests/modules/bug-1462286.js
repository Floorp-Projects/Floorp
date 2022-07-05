let a = registerModule('a', parseModule(`
  export var { ... get } = { x: "foo" };
`));

let m = parseModule("import { get } from 'a'; export { get };");
moduleLink(m);
moduleEvaluate(m)
assertEq(getModuleEnvironmentValue(m, "get").x, "foo");
