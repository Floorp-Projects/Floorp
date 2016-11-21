// |jit-test| need-for-each

(function () {
    for (a = 0; a < 3; a++) {
        for each(b in [0, -0]) {}
    }
})()

// Just test we don't assert.
