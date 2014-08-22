// Destructuring assignment to eval or arguments in destructuring is a SyntaxError
// in strict mode

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
    "[..._]"
];

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
        Function(s);
        assertThrowsInstanceOf(() => Function("'use strict'; " + s), SyntaxError);
    }
}
