// |jit-test| skip-if: !('stackTest' in this)

let a = parseModule(`throw new Error`);
a.declarationInstantiation();
stackTest(function() {
    a.evaluation();
});
