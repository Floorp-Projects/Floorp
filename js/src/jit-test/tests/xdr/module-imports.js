load(libdir + "dummyModuleResolveHook.js");

const count = 10;

let og = parseModule("export let a = 1;");
let bc = codeModule(og);
let a = moduleRepo['a'] = decodeModule(bc);

let s = "";
for (let i = 0; i < count; i++) {
    s += "import { a as i" + i + " } from 'a';\n";
    s += "assertEq(i" + i + ", 1);\n";
}

og = parseModule(s);
bc = codeModule(og);
let b = moduleRepo['b'] = decodeModule(bc);

b.declarationInstantiation();
b.evaluation();


og = parseModule("import * as nsa from 'a'; import * as nsb from 'b';");
bc = codeModule(og);
let m = decodeModule(bc);

m.declarationInstantiation();
m.evaluation();
