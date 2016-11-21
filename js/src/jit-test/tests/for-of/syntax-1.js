// |jit-test| need-for-each

// We correctly reject bogus for-of loop syntax.

load(libdir + "asserts.js");

function assertSyntaxError(code) {
    assertThrowsInstanceOf(function () { Function(code); }, SyntaxError, "Function:" + code);
    assertThrowsInstanceOf(function () { eval(code); }, SyntaxError, "eval:" + code);
    var ieval = eval;
    assertThrowsInstanceOf(function () { ieval(code); }, SyntaxError, "indirect eval:" + code);
}

function test(badForHead) {
    assertSyntaxError(badForHead + " {}");  // apply directly to forHead
    assertSyntaxError("[0 " + badForHead + "];");
}

var a, b, c;
test("for (a in b of c)");
test("for each (a of b)");
test("for (a of b of c)");
test("for (let {a: 1} of b)");
