if (!("oomTest" in this))
    quit();

oomTest(function() {
    for (var i = 0; i < 10; ++i) {
        Promise.resolve().then();
    }
});
