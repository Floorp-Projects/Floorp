// Test many exports.

load(libdir + "dummyModuleResolveHook.js");

const count = 1024;

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
