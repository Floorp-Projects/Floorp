oomTest(function() {
    for (var i = 0; i < 10; ++i) {
        Promise.resolve().then();
    }
});
