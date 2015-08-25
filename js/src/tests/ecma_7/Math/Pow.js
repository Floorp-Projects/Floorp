// |reftest| skip-if(!xulRuntime.shell)
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var BUGNUMBER = 1135708;
var summary = "Implement the exponentiation operator";

print(BUGNUMBER + ": " + summary);

var test = `

// Constant folding
assertEq(2 ** 2 ** 3, 256);
assertEq(1 ** 1 ** 4, 1);

// No folding
var two = 2;
var three = 3;
var four = 4;
assertEq(two ** two ** three, 256);
assertEq(1 ** 1 ** four, 1);

// Operator precedence
assertEq(2 ** 3 / 2 ** 3, 1);
assertEq(2 ** 3 * 2 ** 3, 64);
assertEq(2 ** 3 + 2 ** 3, 16);

// With parentheses
assertEq((2 ** 3) ** 2, 64);
assertEq(2 ** (3 ** 2), 512);

// Assignment operator
var x = 2;
assertEq(x **= 2 ** 3, 256);
assertEq(x, 256);

// Loop to test baseline and ION
for (var i=0; i<10000; i++) {
    assertEq((2 ** 3) ** 2, 64);
    assertEq(2 ** (3 ** 2), 512);
    var x = 2;
    assertEq(x **= 2 ** 3, 256);
    assertEq(x, 256);
}

// Comments should not be confused with exp operator
var a, c, e;
a = c = e = 2;
assertEq(a**/**b**/c/**/**/**d**/e, 16);

// Two stars separated should not parse as exp operator
assertThrows(function() { return Reflect.parse("2 * * 3"); }, SyntaxError);

// Check if error propagation works
var thrower = {
    get value() {
        throw new Error();
    }
};

assertThrowsInstanceOf(function() { return thrower.value ** 2; }, Error);
assertThrowsInstanceOf(function() { return 2 ** thrower.value; }, Error);
assertThrowsInstanceOf(function() { return 2 ** thrower.value ** 2; }, Error);

var convertibleToPrimitive = {
    valueOf: function() {
        throw new Error("oops");
    }
};

assertThrowsInstanceOf(function() { return convertibleToPrimitive ** 3; }, Error);
assertThrowsInstanceOf(function() { return 3 ** convertibleToPrimitive; }, Error);

assertEq(NaN ** 2, NaN);
assertEq(2 ** NaN, NaN);
assertEq(2 ** "3", 8);
assertEq("2" ** 3, 8);

// Reflect.parse generates a correct parse tree for simplest case
var parseTree = Reflect.parse("a ** b");
assertEq(parseTree.body[0].type, "ExpressionStatement");
assertEq(parseTree.body[0].expression.operator, "**");
assertEq(parseTree.body[0].expression.left.name, "a");
assertEq(parseTree.body[0].expression.right.name, "b");

// Reflect.parse generates a tree following the right-associativity rule
var parseTree = Reflect.parse("a ** b ** c");
assertEq(parseTree.body[0].type, "ExpressionStatement");
assertEq(parseTree.body[0].expression.left.name, "a");
assertEq(parseTree.body[0].expression.right.operator, "**");
assertEq(parseTree.body[0].expression.right.left.name, "b");
assertEq(parseTree.body[0].expression.right.right.name, "c");


function assertTrue(v) {
    assertEq(v, true);
}

function assertFalse(v) {
    assertEq(v, false);
}
`;

function exponentiationEnabled() {
    try {
        Function("1 ** 1");
        return true;
    } catch (e if e instanceof SyntaxError) { }
    return false;
}

if (exponentiationEnabled())
    eval(test);

if (typeof reportCompare === "function")
    reportCompare(true, true);
