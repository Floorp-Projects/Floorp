// Test many exports.

const count = 1024;

let moduleRepo = {};
setModuleResolveHook(function(module, specifier) {
    if (specifier in moduleRepo)
        return moduleRepo[specifier];
    throw "Module " + specifier + " not found";
});

let s = "";
for (let i = 0; i < count; i++)
    s += "export let e" + i + " = " + (i * i) + ";\n";
let a = moduleRepo['a'] = parseModule(s);

let b = moduleRepo['b'] = parseModule("import * as ns from 'a'");

b.declarationInstantiation();
b.evaluation();

let ns = a.namespace;
for (let i = 0; i < count; i++)
    assertEq(ns["e" + i], i * i);
