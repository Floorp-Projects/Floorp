load(libdir + "asserts.js");

var functionDeclarations = [
    "function f(){}",
    "function* f(){}",
    "async function f(){}",
];

var varDeclarations = [
    "var f",
    "{ var f; }",
    "for (var f in null);",
    "for (var f of null);",
    "for (var f; ;);",
];

var lexicalDeclarations = [
    "let f;",
    "const f = 0;",
    "class f {};",
];

var imports = [
    "import f from '';",
    "import f, {} from '';",
    "import d, {f} from '';",
    "import d, {f as f} from '';",
    "import d, {foo as f} from '';",
    "import f, * as d from '';",
    "import d, * as f from '';",
    "import {f} from '';",
    "import {f as f} from '';",
    "import {foo as f} from '';",
    "import* as f from '';",
];

var exports = [
    "export var f;",
    ...functionDeclarations.map(fn => `export ${fn};`),
    ...lexicalDeclarations.map(ld => `export ${ld};`),
    ...functionDeclarations.map(fn => `export default ${fn};`),
    "export default class f {};",
];

var redeclarations = [
    ...functionDeclarations,
    ...varDeclarations,
    ...lexicalDeclarations,
    ...imports,
    ...exports,
];

var noredeclarations = [
    ...functionDeclarations.map(fn => `{ ${fn} }`),
    ...lexicalDeclarations.map(ld => `{ ${ld} }`),
    ...["let", "const"].map(ld => `for (${ld} f in null);`),
    ...["let", "const"].map(ld => `for (${ld} f of null);`),
    ...["let", "const"].map(ld => `for (${ld} f = 0; ;);`),
    "export {f};",
    "export {f as f};",
    "export {foo as f}; var foo;",
    "export {f} from '';",
    "export {f as f} from '';",
    "export {foo as f} from '';",
];

for (var decl of functionDeclarations) {
    for (var redecl of redeclarations) {
        assertThrowsInstanceOf(() => {
            parseModule(`
                ${decl}
                ${redecl}
            `);
        }, SyntaxError);

        assertThrowsInstanceOf(() => {
            parseModule(`
                ${redecl}
                ${decl}
            `);
        }, SyntaxError);
    }

    for (var redecl of noredeclarations) {
        parseModule(`
            ${decl}
            ${redecl}
        `);
        parseModule(`
            ${redecl}
            ${decl}
        `);
    }
}
