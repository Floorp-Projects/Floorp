// Destructuring assignment to eval or arguments in destructuring is a SyntaxError

load(libdir + "asserts.js");

var patterns = [
    "[_]",
    "[a, b, _]",
    "[[_]]",
    "[[], [{}, [_]]]",
    "{x:_}",
    "{x:y, z:_}",
    "{0:_}",
    "{_}",
    //"[..._]"
];

// If the assertion below fails, congratulations! It means you have added
// spread operator support to destructuring assignment. Simply uncomment the
// "[..._]" case above. Then delete this comment and assertion.
assertThrowsInstanceOf(() => Function("[...x] = [1]"), ReferenceError);

for (var pattern of patterns) {
    var stmt = pattern + " = obj";
    if (stmt[0] == "{")
        stmt = "(" + stmt + ")";
    stmt += ";"

    // stmt is a legal statement...
    Function(stmt);

    // ...but not if you replace _ with one of these two names.
    for (var name of ["eval", "arguments"]) {
        var s = stmt.replace("_", name);
        assertThrowsInstanceOf(() => Function(s), SyntaxError);
        assertThrowsInstanceOf(() => eval(s), SyntaxError);
        assertThrowsInstanceOf(() => eval("'use strict'; " + s), SyntaxError);
    }
}
