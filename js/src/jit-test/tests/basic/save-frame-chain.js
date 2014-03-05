// Error().stack (ScriptFrameIter) should see through JS_SaveFrameChain.
function gamma() {
    stack = Error().stack;
}
function beta() {
    evaluate("gamma()", {saveFrameChain: true});
}
function alpha() {
    beta();
}
alpha();

assertEq(/alpha/.test(stack), true);
assertEq(/beta/.test(stack), true);
assertEq(/gamma/.test(stack), true);
