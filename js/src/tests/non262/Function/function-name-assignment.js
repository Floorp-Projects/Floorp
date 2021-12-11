var BUGNUMBER = 883377;
var summary = "Anonymous function name should be set based on assignment";

print(BUGNUMBER + ": " + summary);

var fooSymbol = Symbol("foo");
var emptySymbol = Symbol("");
var undefSymbol = Symbol();
var globalVar;

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

function testAssignmentExpression(expr, named) {
    eval(`
    var assignment;
    assignment = ${expr};
    assertEq(assignment.name, named ? "named" : "assignment");

    globalVar = ${expr};
    assertEq(globalVar.name, named ? "named" : "globalVar");

    var obj = { dynamic: null };
    with (obj) {
        dynamic = ${expr};
    }
    assertEq(obj.dynamic.name, named ? "named" : "dynamic");

    (function namedLambda(param1, param2) {
        var assignedToNamedLambda;
        assignedToNamedLambda = namedLambda = ${expr};
        assertEq(namedLambda.name, "namedLambda");
        assertEq(assignedToNamedLambda.name, named ? "named" : "namedLambda");

        param1 = ${expr};
        assertEq(param1.name, named ? "named" : "param1");

        {
            let param1 = ${expr};
            assertEq(param1.name, named ? "named" : "param1");

            param2 = ${expr};
            assertEq(param2.name, named ? "named" : "param2");
        }
    })();

    {
        let nextedLexical1, nextedLexical2;
        {
            let nextedLexical1 = ${expr};
            assertEq(nextedLexical1.name, named ? "named" : "nextedLexical1");

            nextedLexical2 = ${expr};
            assertEq(nextedLexical2.name, named ? "named" : "nextedLexical2");
        }
    }
    `);

    // Not applicable cases: not IsIdentifierRef.
    eval(`
    var inParen;
    (inParen) = ${expr};
    assertEq(inParen.name, named ? "named" : "");
    `);

    // Not applicable cases: not direct RHS.
    if (!expr.includes("=>")) {
        eval(`
        var a = true && ${expr};
        assertEq(a.name, named ? "named" : "");
        `);
    } else {
        // Arrow function cannot be RHS of &&.
        eval(`
        var a = true && (${expr});
        assertEq(a.name, named ? "named" : "");
        `);
    }

    // Not applicable cases: property.
    eval(`
    var obj = {};

    obj.prop = ${expr};
    assertEq(obj.prop.name, named ? "named" : "");

    obj["literal"] = ${expr};
    assertEq(obj["literal"].name, named ? "named" : "");
    `);

    // Not applicable cases: assigned again.
    eval(`
    var tmp = [${expr}];
    assertEq(tmp[0].name, named ? "named" : "");

    var assignment;
    assignment = tmp[0];
    assertEq(assignment.name, named ? "named" : "");
    `);
}
for (var [expr, named] of exprs) {
    testAssignmentExpression(expr, named);
}

function testVariableDeclaration(expr, named) {
    eval(`
    var varDecl = ${expr};
    assertEq(varDecl.name, named ? "named" : "varDecl");
    `);
}
for (var [expr, named] of exprs) {
    testVariableDeclaration(expr, named);
}

function testLexicalBinding(expr, named) {
    eval(`
    let lexical = ${expr};
    assertEq(lexical.name, named ? "named" : "lexical");

    const constLexical = ${expr};
    assertEq(constLexical.name, named ? "named" : "constLexical");
    `);
}
for (var [expr, named] of exprs) {
    testLexicalBinding(expr, named);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
