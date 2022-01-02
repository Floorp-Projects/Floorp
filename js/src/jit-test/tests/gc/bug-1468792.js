// |jit-test| error: ReferenceError

loadFile(`
  gczeal(2,9);
  evaluate(\`
    reportCompare(expect, actual, summary);
  \`);
`);
function loadFile(lfVarx) {
    try {
        evaluate(lfVarx);
    } catch (lfVare) {}
}
eval("(function(){({6953421313:0})})")();
function f() {
    x[6953421313] = "a";
}
f();
