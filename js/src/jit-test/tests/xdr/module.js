// |jit-test|

load(libdir + "asserts.js");

async function parseAndEvaluate(source) {
    let og = parseModule(source);
    let bc = codeModule(og);
    let m = decodeModule(bc);
    m.declarationInstantiation();
    await m.evaluation();
    return m;
}

(async () => {
  // Check the evaluation of an empty module succeeds.
  await parseAndEvaluate("");
})();

(async () => {
  // Check that evaluation returns evaluation promise,
  // and promise is always the same.
  let og = parseModule("1");
  let bc = codeModule(og);
  let m = decodeModule(bc);
  m.declarationInstantiation();
  assertEq(typeof m.evaluation(), "object");
  assertEq(m.evaluation() instanceof Promise, true);
  assertEq(m.evaluation(), m.evaluation());
})();

(async () => {
  // Check top level variables are initialized by evaluation.
  let og = parseModule("export var x = 2 + 2;");
  let bc = codeModule(og);
  let m = decodeModule(bc);
  assertEq(typeof getModuleEnvironmentValue(m, "x"), "undefined");
  m.declarationInstantiation();
  await m.evaluation();
  assertEq(getModuleEnvironmentValue(m, "x"), 4);
})();

(async () => {
  let m = await parseAndEvaluate("export let x = 2 * 3;");
  assertEq(getModuleEnvironmentValue(m, "x"), 6);

  // Set up a module to import from.
  let og = parseModule(`var x = 1;
                          export { x };
                          export default 2;
                          export function f(x) { return x + 1; }`);
  let bc = codeModule(og);
  let a = registerModule('a', decodeModule(bc));

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

  // Test default import
  m = await parseAndEvaluate("import a from 'a'; export { a };")
  assertEq(getModuleEnvironmentValue(m, "a"), 2);

  // Test named import
  m = await parseAndEvaluate("import { x as y } from 'a'; export { y };")
  assertEq(getModuleEnvironmentValue(m, "y"), 1);

  // Call exported function
  m = await parseAndEvaluate("import { f } from 'a'; export let x = f(3);")
  assertEq(getModuleEnvironmentValue(m, "x"), 4);

  // Import access in functions
  m = await parseAndEvaluate("import { x } from 'a'; function f() { return x; }")
  let f = getModuleEnvironmentValue(m, "f");
  assertEq(f(), 1);
})();
drainJobQueue();
