// |trace-test| error: TypeError
(function () {
    var b = e
    for (var [e] = b in w) c
})()

/* Don't assert. */

