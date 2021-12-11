
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
    "^", "function", ("(?:" + propertyName + ")?"), "\\(", "\\)", "\\{", "\\[native code\\]", "\\}", "$"
].join("\\s*"));


// Bound functions are considered built-ins.
reportMatch(nativeCode, function(){}.bind().toString());
reportMatch(nativeCode, function fn(){}.bind().toString());

// Built-ins which are well-known intrinsic objects.
reportMatch(nativeCode, Array.toString());
reportMatch(nativeCode, Object.prototype.toString.toString());
reportMatch(nativeCode, decodeURI.toString());

// Other built-in functions.
reportMatch(nativeCode, Math.asin.toString());
reportMatch(nativeCode, String.prototype.blink.toString());
reportMatch(nativeCode, RegExp.prototype[Symbol.split].toString());

// Built-in getter functions.
reportMatch(nativeCode, Object.getOwnPropertyDescriptor(RegExp.prototype, "flags").get.toString());
reportMatch(nativeCode, Object.getOwnPropertyDescriptor(Object.prototype, "__proto__").get.toString());

// Built-in setter functions.
reportMatch(nativeCode, Object.getOwnPropertyDescriptor(Object.prototype, "__proto__").set.toString());
