// Test importing a namespace many times.

load(libdir + "dummyModuleResolveHook.js");

const count = 1024;

let a = moduleRepo['a'] = parseModule("export let a = 1;");

let s = "";
for (let i = 0; i < count; i++) {
    s += "import * as ns" + i + " from 'a';\n";
    s += "assertEq(ns" + i + ".a, 1);\n";
}
let b = moduleRepo['b'] = parseModule(s);

b.declarationInstantiation();
b.evaluation();
