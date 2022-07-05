const count = 10;

let stencil = compileToStencilXDR("export let a = 1;", {module: true});
let m = instantiateModuleStencilXDR(stencil);
let a = registerModule('a', m);

let s = "";
for (let i = 0; i < count; i++) {
    s += "import { a as i" + i + " } from 'a';\n";
    s += "assertEq(i" + i + ", 1);\n";
}

stencil = compileToStencilXDR(s, {module: true});
m = instantiateModuleStencilXDR(stencil);
let b = registerModule('b', m);

moduleLink(b);
moduleEvaluate(b);


stencil = compileToStencilXDR("import * as nsa from 'a'; import * as nsb from 'b';", {module: true});
m = instantiateModuleStencilXDR(stencil);

moduleLink(m);
moduleEvaluate(m);
