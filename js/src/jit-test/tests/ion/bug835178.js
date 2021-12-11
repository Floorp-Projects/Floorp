function boo() { return foo.arguments[0] }
function foo(a,b,c) { if (a == 0) {a = 2; return boo();} return a }
function inlined() { return foo.apply({}, arguments); }
function test(a,b,c) { return inlined(a,b,c) }

assertEq(test(1,2,3), 1);
assertEq(test(0,2,3), 2);

function g(a) { if (g.arguments[1]) return true; return false; };
function f() { return g(false, true); };
function h() { return f(false, false); }

assertEq(h(false, false), true);
assertEq(h(false, false), true);

function g2(a) { if (a) { if (g2.arguments[1]) return true; return false; } return true; };
function f2(a) { return g2(a, true); };
function h2(a) { return f2(a, false); }

assertEq(h2(false, false), true);
assertEq(h2(true, false), true);

// Currently disabled for now, but in testsuite to be sure
function g3(a) { return a };
function f3(a) { a = 3; return g3.apply({}, arguments); };
function h3(a) { return f3(a); }

assertEq(h3(0), 3);
assertEq(h3(0), 3);

