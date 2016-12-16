// |jit-test| error: ReferenceError

(function() {
    for (var i = 0; i < 4; ++i) {
        if (i % 3 == 0) {
            for (var x in y) {}
        } else {
            continue;
        }
    }
})()

