
load(libdir + "asserts.js");
load(libdir + "dummyModuleResolveHook.js");

function parseAndEvaluate(source) {
    let og = parseModule(source);
    let bc = codeModule(og);
    let m = decodeModule(bc);
    m.declarationInstantiation();
    m.evaluation();
    return m;
}

// Check the evaluation of an empty module succeeds.
parseAndEvaluate("");

// Check evaluation returns evaluation result the first time, then undefined.
let og = parseModule("1");
let bc = codeModule(og);
let m = decodeModule(bc);
m.declarationInstantiation();
assertEq(m.evaluation(), undefined);
assertEq(typeof m.evaluation(), "undefined");

// Check top level variables are initialized by evaluation.
og = parseModule("export var x = 2 + 2;");
bc = codeModule(og);
m = decodeModule(bc);
assertEq(typeof getModuleEnvironmentValue(m, "x"), "undefined");
m.declarationInstantiation();
m.evaluation();
assertEq(getModuleEnvironmentValue(m, "x"), 4);

m = parseAndEvaluate("export let x = 2 * 3;");
assertEq(getModuleEnvironmentValue(m, "x"), 6);

// Set up a module to import from.
og = parseModule(`var x = 1;
                        export { x };
                        export default 2;
                        export function f(x) { return x + 1; }`);
bc = codeModule(og);
a = moduleRepo['a'] = decodeModule(bc);

// Check we can evaluate top level definitions.
parseAndEvaluate("var foo = 1;");
parseAndEvaluate("let foo = 1;");
parseAndEvaluate("const foo = 1");
parseAndEvaluate("function foo() {}");
parseAndEvaluate("class foo { constructor() {} }");

// Check we can evaluate all module-related syntax.
parseAndEvaluate("export var foo = 1;");
parseAndEvaluate("export let foo = 1;");
parseAndEvaluate("export const foo = 1;");
parseAndEvaluate("var x = 1; export { x };");
parseAndEvaluate("export default 1");
parseAndEvaluate("export default function() {};");
parseAndEvaluate("export default function foo() {};");
parseAndEvaluate("import a from 'a';");
parseAndEvaluate("import { x } from 'a';");
parseAndEvaluate("import * as ns from 'a';");
parseAndEvaluate("export * from 'a'");
parseAndEvaluate("export default class { constructor() {} };");
parseAndEvaluate("export default class foo { constructor() {} };");

// Test default import
m = parseAndEvaluate("import a from 'a'; export { a };")
assertEq(getModuleEnvironmentValue(m, "a"), 2);

// Test named import
m = parseAndEvaluate("import { x as y } from 'a'; export { y };")
assertEq(getModuleEnvironmentValue(m, "y"), 1);

// Call exported function
m = parseAndEvaluate("import { f } from 'a'; export let x = f(3);")
assertEq(getModuleEnvironmentValue(m, "x"), 4);

// Import access in functions
m = parseAndEvaluate("import { x } from 'a'; function f() { return x; }")
let f = getModuleEnvironmentValue(m, "f");
assertEq(f(), 1);
