// |jit-test| error: ReferenceError

let moduleRepo = {};
setModuleResolveHook(function(module, specifier) {
    return moduleRepo[specifier];
});
assertEq = function(a, b) {}
evaluate(`
let a = moduleRepo['a'] = parseModule(
    'export var a = 1;'
);
let b = moduleRepo['b'] = parseModule(
    \`import * as ns from 'a';
     export var x = ns.a + ns.b;\`
);
b.declarationInstantiation();
let ns = getModuleEnvironmentValue(b, "ns");
assertEq(ns.a, 1);
while ( t.ArrayType() ) 1;
`);
