// Error().stack (ScriptFrameIter) is no longer context-bound.
function beta() {
    evaluate("function gamma() {\nstack = Error().stack;\n };\n gamma();", {newContext: true});
}
function alpha() {
    beta();
}
alpha();
assertEq(/alpha/.test(stack), true);
assertEq(/beta/.test(stack), true);
assertEq(/gamma/.test(stack), true);
assertEq(/delta/.test(stack), false);

function delta() {
    evaluate("stack = Error().stack", {newContext: true});
}
delta();
assertEq(/alpha/.test(stack), false);
assertEq(/beta/.test(stack), false);
assertEq(/gamma/.test(stack), false);
assertEq(/delta/.test(stack), true);
