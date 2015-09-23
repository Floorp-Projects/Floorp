// Exercise ModuleEvaluation() concrete method.

load(libdir + "asserts.js");

let moduleRepo = new Map();
setModuleResolveHook(function(module, specifier) {
    if (specifier in moduleRepo)
        return moduleRepo[specifier];
    throw "Module " + specifier + " not found";
});

function parseAndEvaluate(source) {
    let m = parseModule(source);
    m.declarationInstantiation();
    return m.evaluation();
}

// Check the evaluation of an empty module succeeds.
assertEq(typeof parseAndEvaluate(""), "undefined");

// Check evaluation returns evaluation result the first time, then undefined.
let m = parseModule("1");
m.declarationInstantiation();
assertEq(m.evaluation(), 1);
assertEq(typeof m.evaluation(), "undefined");

// Check top level variables are initialized by evaluation.
m = parseModule("export var x = 2 + 2;");
assertEq(typeof m.initialEnvironment.x, "undefined");
m.declarationInstantiation();
m.evaluation();
assertEq(m.environment.x, 4);

m = parseModule("export let x = 2 * 3;");
m.declarationInstantiation();
m.evaluation();
assertEq(m.environment.x, 6);

// Set up a module to import from.
let a = moduleRepo['a'] =
parseModule(`var x = 1;
             export { x };
             export default 2;
             export function f(x) { return x + 1; }`);

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
parseAndEvaluate("export default class { constructor() {} };");
parseAndEvaluate("export default function() {};");
parseAndEvaluate("export default class foo { constructor() {} };");
parseAndEvaluate("export default function foo() {};");
parseAndEvaluate("import a from 'a';");
parseAndEvaluate("import { x } from 'a';");
parseAndEvaluate("import * as ns from 'a';");
parseAndEvaluate("export * from 'a'");

// Test default import
m = parseModule("import a from 'a'; a;")
m.declarationInstantiation();
assertEq(m.evaluation(), 2);

// Test named import
m = parseModule("import { x as y } from 'a'; y;")
m.declarationInstantiation();
assertEq(m.evaluation(), 1);

// Call exported function
m = parseModule("import { f } from 'a'; f(3);")
m.declarationInstantiation();
assertEq(m.evaluation(), 4);

// Test importing an indirect export
moduleRepo['b'] = parseModule("export { x as z } from 'a';");
assertEq(parseAndEvaluate("import { z } from 'b'; z"), 1);

// Test cyclic dependencies
moduleRepo['c1'] = parseModule("export var x = 1; export {y} from 'c2'");
moduleRepo['c2'] = parseModule("export var y = 2; export {x} from 'c1'");
assertDeepEq(parseAndEvaluate(`import { x as x1, y as y1 } from 'c1'; 
                               import { x as x2, y as y2 } from 'c2';
                               [x1, y1, x2, y2]`),
             [1, 2, 1, 2]);

// Import access in functions
m = parseModule("import { x } from 'a'; function f() { return x; }")
m.declarationInstantiation();
m.evaluation();
assertEq(m.environment.f(), 1);
