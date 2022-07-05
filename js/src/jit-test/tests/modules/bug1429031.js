// |jit-test| error: ReferenceError

assertEq = function(a, b) {}
evaluate(`
let a = registerModule('a', parseModule(
    'export var a = 1;'
));
let b = registerModule('b', parseModule(
    \`import * as ns from 'a';
     export var x = ns.a + ns.b;\`
));
moduleLink(b);
let ns = getModuleEnvironmentValue(b, "ns");
assertEq(ns.a, 1);
while ( t.ArrayType() ) 1;
`);
