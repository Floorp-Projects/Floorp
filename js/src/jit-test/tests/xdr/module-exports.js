const count = 10;

let s = "";
for (let i = 0; i < count; i++)
    s += "export let e" + i + " = " + (i * i) + ";\n";
let stencil = compileToStencilXDR(s, {module: true});
let m = instantiateModuleStencilXDR(stencil);
let a = registerModule('a', m);

stencil = compileToStencilXDR("import * as ns from 'a'", {module: true});
m = instantiateModuleStencilXDR(stencil);
let b = registerModule('b', m);

moduleLink(b);
moduleEvaluate(b);

let ns = a.namespace;
for (let i = 0; i < count; i++)
    assertEq(ns["e" + i], i * i);
