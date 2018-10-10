// |jit-test| skip-if: !('oomTest' in this)

oomTest(function() {
    for (var i = 0; i < 10; ++i) {
        Promise.resolve().then();
    }
});
