load(libdir + "dummyModuleResolveHook.js");
const count = 10;

let s = "";
for (let i = 0; i < count; i++)
    s += "export let e" + i + " = " + (i * i) + ";\n";
let og = parseModule(s);
let bc = codeModule(og);
let a = moduleRepo['a'] = decodeModule(bc);

og = parseModule("import * as ns from 'a'");
bc = codeModule(og);
let b = moduleRepo['b'] = decodeModule(bc);

b.declarationInstantiation();
b.evaluation();

let ns = a.namespace;
for (let i = 0; i < count; i++)
    assertEq(ns["e" + i], i * i);
