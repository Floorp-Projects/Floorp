// for-of does not trigger the JS 1.7 for-in destructuring special case.

version(170);

var data = [[1, 2, 3], [4, 5, 6, 7]];

function test(vars, expr, result) {
    var s = '';
    eval("for (" + vars + " of data) s += (" + expr + ") + ';';");
    assertEq(s, result);
}

for (var prefix of ["var ", "let ", ""]) {
    test(prefix + "[a, b, c]",
         "a + ',' + b + ',' + c",
         "1,2,3;4,5,6;");
}

test("var [a]", "a", "1;4;");
test("var {length: len}", "len", "3;4;");
test("var {length}", "length", "3;4;");
test("{}", "0", "0;0;");

