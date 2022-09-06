// |jit-test| skip-if: helperThreadCount() === 0

// Overwrite built-in parseModule with off-thread module parser.
function parseModule(source) {
    offThreadCompileModuleToStencil(source);
    var stencil = finishOffThreadStencil();
    return instantiateModuleStencil(stencil);
}

// Test case derived from: js/src/jit-test/tests/modules/many-imports.js

// Test importing an import many times.

const count = 1024;

let a = registerModule('a', parseModule("export let a = 1;"));

let s = "";
for (let i = 0; i < count; i++) {
    s += "import { a as i" + i + " } from 'a';\n";
    s += "assertEq(i" + i + ", 1);\n";
}
let b = registerModule('b', parseModule(s));

moduleLink(b);
moduleEvaluate(b);
