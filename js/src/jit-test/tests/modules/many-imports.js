// Test importing an import many times.

load(libdir + "dummyModuleResolveHook.js");

const count = 1024;

let a = moduleRepo['a'] = parseModule("export let a = 1;");

let s = "";
for (let i = 0; i < count; i++) {
    s += "import { a as i" + i + " } from 'a';\n";
    s += "assertEq(i" + i + ", 1);\n";
}
let b = moduleRepo['b'] = parseModule(s);

b.declarationInstantiation();
b.evaluation();
