// The decompiler correctly handles for-of loops.

function tokens(code) {
    var arr = [];
    var s = code.replace(/\w+|[^\s]/g, function (tok) { arr.push(tok); return ""; });
    assertEq(s.trim(), "", "tokens() should find all tokens in code: " + uneval(code));
    return arr;
}

function test(code) {
    var before = "function f() { " + code + " }";
    var after = eval("(" + before + ")").toString();
    assertEq(tokens(before).join(" "), tokens(after).join(" "), "decompiler failed to round-trip");
}

// statements
test("for (a of b) { f(a); }");
test("for (a of b) { f(a); g(a); }");

// for-of with "in" operator nearby
test("for (a of b in c ? c : c.items()) { f(a); }");

// destructuring
test("for ([a, b] of c) { a.m(b); }");

// for-let-of
test("for (let a of b) { f(a); }");
test("for (let [a, b] of c) { a.m(b); }");
