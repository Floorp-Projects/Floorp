// Two Environments nested in the same runtime scope share the correct tail of their parent chains.

// The compiler must be allowed to elide empty scopes and so forth, so this
// test does not check the number of unshared Environments. Instead, each test
// case identifies the expected innermost shared scope by the name of a
// variable in it.

var g = newGlobal();
g.eval("function h() { debugger; }");
var dbg = Debugger(g);
var hits, name, shared, unshared;
dbg.onDebuggerStatement = function (hframe) {
    var frame = hframe.older;

    // Find name in frame.environment.
    var env, child = null;
    for (env = frame.environment; env !== null; env = env.parent) {
        if (env.names().indexOf(name) != -1)
            break;
        child = env;
    }
    assertEq(env !== null, true, "expected '" + name + "' to be in scope");
    assertEq(env, frame.environment.find(name),
             "env.find should find the same frame as the written out search");

    if (hits === 0) {
        // First hit.
        shared = env;
        unshared = child;
    } else {
        // Subsequent hit.
        assertEq(env, shared, "the environment containing '" + name + "' should be shared");
        assertEq(child === null || unshared === null || unshared !== child, true,
                "environments nested within the one containing '" + name + "' should not be shared");
    }
    hits++;
};

function test(sharedName, expectedHits, code) {
    hits = 0;
    name = sharedName;
    shared = unshared = undefined;
    g.eval(code);
    assertEq(hits, expectedHits);
}

// Basic test cases.
//
// (The stray "a = b" assignments in these tests are to inhibit the flat closure
// optimization, which Environments expose. There's nothing really wrong with
// the optimization or with the debugger exposing it, but that's not what we
// want to test here.)

test("q", 2, "var q = function (a) { h(); }; q(1); q(2);");
test("a", 2, "q = function (a) { (function (b) { h(); a = b; })(2); h(); }; q(1);");
test("a", 2, "q = function (a) { h(); return function (b) { h(); a = b; }; }; q(1)(2);");
test("n", 3, "q = function (n) { for (var i = 0; i < n; i++) { let (j = i) { h(); } } }; q(3);");

// A function with long dynamic and static chains.
var N = 80;

var code = "function f" + N + "(a" + N + ") {\neval('a0 + a1'); h();\n}\n";
for (var i = N; --i >= 0;) {
    var call = "f" + (i + 1) + "(a" + i + " - 1);\n";
    code = ("function f" + i + "(a" + i + ") {\n" +
            code +
            call +
            "if (a" + i + " === 0) " + call +
            "}\n");
}

g.eval(code);
test("a0", 2, "f0(0);");
test("a17", 2, "f0(17);");
test("a" + (N-2), 2, "f0(" + (N-2) + ");");
test("a" + (N-1), 2, "f0(" + (N-1) + ");");

// A function with a short dynamic chain and a long static chain.
N = 60;

function DeepStaticShallowDynamic(i, n) {
    var code = "function f" + i + "(a" + i + ") {\n";
    if (i >= n)
        code += "eval('a1 + a2'); h();\n";
    else
        code += "return " + DeepStaticShallowDynamic(i+1, n) + ";\n";
    code += "}";
    return code;
}
g.eval(DeepStaticShallowDynamic(1, N));

function range(start, stop) {
    for (var i = start; i < stop; i++)
        yield i;
}

function DSSDsplit(s) {
    return ("var mid = f1" + ["(" + i + ")" for (i in range(0, s))].join("") + ";\n" +
            "mid" +          ["(" + i + ")" for (i in range(s, N))].join("") + ";\n" +
            "mid" +          ["(" + i + ")" for (i in range(s, N))].join("") + ";\n");
}

test("a1", 2, DSSDsplit(1));
test("a17", 2, DSSDsplit(17));
test("a" + (N-2), 2, DSSDsplit(N-2));
test("a" + (N-1), 2, DSSDsplit(N-1));
