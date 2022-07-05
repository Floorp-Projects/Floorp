// |jit-test| skip-if: !('stackTest' in this)

stackTest(function() {
    let m = parseModule(``);
    moduleLink(m);
});
