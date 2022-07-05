// |jit-test| skip-if: !('stackTest' in this)

let a = parseModule(`throw new Error`);
moduleLink(a);
stackTest(function() {
    moduleEvaluate(a);
});
