// |jit-test|
// Exercise ModuleEvaluation() concrete method.

load(libdir + "asserts.js");

async function parseAndEvaluate(source) {
    let m = parseModule(source);
    m.declarationInstantiation();
    await m.evaluation();
    return m;
}

// Check the evaluation of an empty module succeeds.
(async () => {
  await parseAndEvaluate("");
})();

(async () => {
  // Check that evaluation returns evaluation promise,
  // and promise is always the same.
  let m = parseModule("1");
  m.declarationInstantiation();
  assertEq(typeof m.evaluation(), "object");
  assertEq(m.evaluation() instanceof Promise, true);
  assertEq(m.evaluation(), m.evaluation());
  await m.evaluation();
})();

(async () => {
  // Check top level variables are initialized by evaluation.
  let m = parseModule("export var x = 2 + 2;");
  assertEq(typeof getModuleEnvironmentValue(m, "x"), "undefined");
  m.declarationInstantiation();
  await m.evaluation();
  assertEq(getModuleEnvironmentValue(m, "x"), 4);
})();

(async () => {
  let m = parseModule("export let x = 2 * 3;");
  m.declarationInstantiation();
  await m.evaluation();
  assertEq(getModuleEnvironmentValue(m, "x"), 6);
})();

// Set up a module to import from.
let a = registerModule('a',
    parseModule(`var x = 1;
                 export { x };
                 export default 2;
                 export function f(x) { return x + 1; }`));

(async () => {
  // Check we can evaluate top level definitions.
  await parseAndEvaluate("var foo = 1;");
  await parseAndEvaluate("let foo = 1;");
  await parseAndEvaluate("const foo = 1");
  await parseAndEvaluate("function foo() {}");
  await parseAndEvaluate("class foo { constructor() {} }");

  // Check we can evaluate all module-related syntax.
  await parseAndEvaluate("export var foo = 1;");
  await parseAndEvaluate("export let foo = 1;");
  await parseAndEvaluate("export const foo = 1;");
  await parseAndEvaluate("var x = 1; export { x };");
  await parseAndEvaluate("export default 1");
  await parseAndEvaluate("export default function() {};");
  await parseAndEvaluate("export default function foo() {};");
  await parseAndEvaluate("import a from 'a';");
  await parseAndEvaluate("import { x } from 'a';");
  await parseAndEvaluate("import * as ns from 'a';");
  await parseAndEvaluate("export * from 'a'");
  await parseAndEvaluate("export default class { constructor() {} };");
  await parseAndEvaluate("export default class foo { constructor() {} };");
})();

(async () => {
  // Test default import
  let m = parseModule("import a from 'a'; export { a };")
  m.declarationInstantiation();
  await m.evaluation()
  assertEq(getModuleEnvironmentValue(m, "a"), 2);
})();

(async () => {
  // Test named import
  let m = parseModule("import { x as y } from 'a'; export { y };")
  m.declarationInstantiation();
  await m.evaluation();
  assertEq(getModuleEnvironmentValue(m, "y"), 1);
})();

(async () => {
  // Call exported function
  let m = parseModule("import { f } from 'a'; export let x = f(3);")
  m.declarationInstantiation();
  await m.evaluation();
  assertEq(getModuleEnvironmentValue(m, "x"), 4);
})();

(async () => {
  // Test importing an indirect export
  registerModule('b', parseModule("export { x as z } from 'a';"));
  let m = await parseAndEvaluate("import { z } from 'b'; export { z }");
  assertEq(getModuleEnvironmentValue(m, "z"), 1);
})();

(async () => {
  // Test cyclic dependencies
  registerModule('c1', parseModule("export var x = 1; export {y} from 'c2'"));
  registerModule('c2', parseModule("export var y = 2; export {x} from 'c1'"));
  let m = await parseAndEvaluate(`import { x as x1, y as y1 } from 'c1';
                        import { x as x2, y as y2 } from 'c2';
                        export let z = [x1, y1, x2, y2]`);
  assertDeepEq(getModuleEnvironmentValue(m, "z"), [1, 2, 1, 2]);
})();

(async () => {
  // Import access in functions
  let m = await parseModule("import { x } from 'a'; function f() { return x; }")
  m.declarationInstantiation();
  m.evaluation();
  let f = getModuleEnvironmentValue(m, "f");
  assertEq(f(), 1);
})();
drainJobQueue();
