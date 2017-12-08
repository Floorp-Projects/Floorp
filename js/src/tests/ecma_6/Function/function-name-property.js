var BUGNUMBER = 883377;
var summary = "Anonymous function name should be set based on property name";

print(BUGNUMBER + ": " + summary);

var fooSymbol = Symbol("foo");
var emptySymbol = Symbol("");
var undefSymbol = Symbol();

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

function testPropertyDefinition(expr, named) {
    var obj = eval(`({
        prop: ${expr},
        "literal": ${expr},
        "": ${expr},
        5: ${expr},
        0.4: ${expr},
        [Symbol.iterator]: ${expr},
        [fooSymbol]: ${expr},
        [emptySymbol]: ${expr},
        [undefSymbol]: ${expr},
        [/a/]: ${expr},
    })`);
    assertEq(obj.prop.name, named ? "named" : "prop");
    assertEq(obj["literal"].name, named ? "named" : "literal");
    assertEq(obj[""].name, named ? "named" : "");
    assertEq(obj[5].name, named ? "named" : "5");
    assertEq(obj[0.4].name, named ? "named" : "0.4");
    assertEq(obj[Symbol.iterator].name, named ? "named" : "[Symbol.iterator]");
    assertEq(obj[fooSymbol].name, named ? "named" : "[foo]");
    assertEq(obj[emptySymbol].name, named ? "named" : "[]");
    assertEq(obj[undefSymbol].name, named ? "named" : "");
    assertEq(obj[/a/].name, named ? "named" : "/a/");

    // Not applicable cases: __proto__.
    obj = {
        __proto__: function() {}
    };
    assertEq(obj.__proto__.name, "");
}
for (var [expr, named] of exprs) {
    testPropertyDefinition(expr, named);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
