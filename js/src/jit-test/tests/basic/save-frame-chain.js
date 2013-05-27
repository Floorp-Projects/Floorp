// Error().stack (ScriptFrameIter) should not see through JS_SaveFrameChain.
function h() {
    stack = Error().stack;
}
function g() {
    evaluate("h()", {saveFrameChain: true});
}
function f() {
    g();
}
f();
var lines = stack.split("\n");
assertEq(lines.length, 3);
assertEq(lines[0].startsWith("h@"), true);
assertEq(lines[1].startsWith("@@evaluate"), true);
assertEq(lines[2], "");
