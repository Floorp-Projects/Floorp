let a = parseModule(`throw new Error`);
moduleLink(a);
stackTest(function() {
    moduleEvaluate(a);
});
