(function() {
    var x = [null, [], null];
    for (var i = 0; i != x.length; ++i) {
        eval("for (var b = 0; b < 2; ++b);", x[i]);
    }
})();

// Just test that we don't crash.
