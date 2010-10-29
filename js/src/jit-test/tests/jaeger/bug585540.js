try {
    (function () {
        gczeal(2)()
    })()
} catch (e) {}
(function () {
    for (y in [/x/, Boolean, Boolean, 0, Boolean]) {
        [Math.floor(this)].some(function () {})
    }
})()

/* Don't crash. */

