// Repeats the test from 'Function/function-toString-builtin.js' and additionally
// verifies the name matches the expected value.
//
// This behaviour is not required by the ECMAScript standard.

// Greatly (!) simplified patterns for the PropertyName production.
var propertyName = [
    // PropertyName :: LiteralPropertyName :: IdentifierName
    "\\w+",

    // PropertyName :: LiteralPropertyName :: StringLiteral
    "(?:'[^']*')",
    "(?:\"[^\"]*\")",

    // PropertyName :: LiteralPropertyName :: NumericLiteral
    "\\d+",

    // PropertyName :: ComputedPropertyName
    "(?:\\[[^\\]]+\\])",
].join("|")

var nativeCode = RegExp([
    "^", "function", ("(" + propertyName + ")?"), "\\(", "\\)", "\\{", "\\[native code\\]", "\\}", "$"
].join("\\s*"));

function assertFunctionName(fun, expected) {
    var match = nativeCode.exec(fun.toString());
    assertEq(match[1], expected);
}

// Bound functions are considered built-ins.
assertFunctionName(function(){}.bind(), undefined);
assertFunctionName(function fn(){}.bind(), undefined);

// Built-ins which are well-known intrinsic objects.
assertFunctionName(Array, "Array");
assertFunctionName(Object.prototype.toString, "toString");
assertFunctionName(decodeURI, "decodeURI");

// Other built-in functions.
assertFunctionName(Math.asin, "asin");
assertFunctionName(String.prototype.blink, "blink");
assertFunctionName(RegExp.prototype[Symbol.split], "[Symbol.split]");

// Built-in getter functions.
assertFunctionName(Object.getOwnPropertyDescriptor(RegExp.prototype, "flags").get, "flags");
assertFunctionName(Object.getOwnPropertyDescriptor(Object.prototype, "__proto__").get, "__proto__");

// Built-in setter functions.
assertFunctionName(Object.getOwnPropertyDescriptor(Object.prototype, "__proto__").set, "__proto__");

if (typeof reportCompare === "function")
    reportCompare(0, 0);
