// Test errors due to duplicate exports
load(libdir + "asserts.js");

function testSyntaxError(source) {
    assertThrowsInstanceOf(function () {
        parseModule(source);
    }, SyntaxError);
}

// SyntexError due to duplicate exports
testSyntaxError("export var v; export var v;");
testSyntaxError("export var x, y, z; export var y;");
testSyntaxError("export let v; var w; export {w as v};");
testSyntaxError("export const v; var w; export {w as v};");
testSyntaxError("export var v; let w; export {w as v};");
testSyntaxError("export var v; const w; export {w as v};");
testSyntaxError("export default 1; export default 2;");
testSyntaxError("export default 1; export default function() {};");
testSyntaxError("export default 1; export default function foo() {};");
testSyntaxError("var v; export {v}; export {v};");
testSyntaxError("var v, x; export {v}; export {x as v};");
testSyntaxError("export default 1; export default export class { constructor() {} };");
testSyntaxError("export default 1; export default export class foo { constructor() {} };");

// SyntaxError due to redeclared binding
testSyntaxError("export let v; export let v;");
testSyntaxError("export let x, y, z; export let y;");
testSyntaxError("export const v = 0; export const v = 0;");
testSyntaxError("export const x = 0, y = 0, z = 0; export const y = 0;");
testSyntaxError("export var v; export let v;");
testSyntaxError("export var v; export const v = 0;");
testSyntaxError("export let v; export const v;");
