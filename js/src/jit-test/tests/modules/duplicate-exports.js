// Test errors due to duplicate exports

load(libdir + "asserts.js");

function testSyntaxError(source) {
    assertThrowsInstanceOf(function () {
        parseModule(source);
    }, SyntaxError);
}

testSyntaxError("export var v; export var v;");
testSyntaxError("export var x, y, z; export var y;");
testSyntaxError("export default 1; export default 2;");
testSyntaxError("export var default; export default 1;");
testSyntaxError("export var default; export default function() {};");
testSyntaxError("export var default; export default function foo() {};");
testSyntaxError("var v; export {v}; export {v};");
testSyntaxError("var v, x; export {v}; export {x as v};");
if (classesEnabled()) {
    testSyntaxError("export var default; export default export class { constructor() {} };");
    testSyntaxError("export var default; export default export class foo { constructor() {} };");
}
