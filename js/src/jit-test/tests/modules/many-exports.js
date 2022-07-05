// Test many exports.

const count = 1024;

let s = "";
for (let i = 0; i < count; i++)
    s += "export let e" + i + " = " + (i * i) + ";\n";
let a = registerModule('a', parseModule(s));

let b = registerModule('b', parseModule("import * as ns from 'a'"));

moduleLink(b);
moduleEvaluate(b);

let ns = a.namespace;
for (let i = 0; i < count; i++)
    assertEq(ns["e" + i], i * i);
