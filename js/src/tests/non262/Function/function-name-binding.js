var BUGNUMBER = 883377;
var summary = "Anonymous function name should be set based on binding pattern";

print(BUGNUMBER + ": " + summary);

var exprs = [
    ["function() {}", false],
    ["function named() {}", true],
    ["function*() {}", false],
    ["function* named() {}", true],
    ["async function() {}", false],
    ["async function named() {}", true],
    ["() => {}", false],
    ["async () => {}", false],
    ["class {}", false],
    ["class named {}", true],
];

function testAssignmentProperty(expr, named) {
    var f = eval(`(function({ prop1 = ${expr} }) { return prop1; })`);
    assertEq(f({}).name, named ? "named" : "prop1");

    eval(`
    var { prop1 = ${expr} } = {};
    assertEq(prop1.name, named ? "named" : "prop1");
    `);
}
for (var [expr, named] of exprs) {
    testAssignmentProperty(expr, named);
}

function testAssignmentElement(expr, named) {
    var f = eval(`(function([elem1 = ${expr}]) { return elem1; })`);
    assertEq(f([]).name, named ? "named" : "elem1");

    eval(`
    var [elem1 = ${expr}] = [];
    assertEq(elem1.name, named ? "named" : "elem1");
    `);
}
for (var [expr, named] of exprs) {
    testAssignmentElement(expr, named);
}

function testSingleNameBinding(expr, named) {
    var f = eval(`(function(param1 = ${expr}) { return param1; })`);
    assertEq(f().name, named ? "named" : "param1");
}
for (var [expr, named] of exprs) {
    testSingleNameBinding(expr, named);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
