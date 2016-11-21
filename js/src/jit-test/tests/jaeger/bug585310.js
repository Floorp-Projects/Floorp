// |jit-test| need-for-each

gczeal(2)
try {
    (function () {
        for each(l in [0, 0, 0]) {
            print(''.replace(function () {}))
        }
    })()
} catch (e) {}

x = gczeal(2)
try {
    (function () {
        for each(d in [x, x, x]) {
            'a'.replace(/a/, function () {})
        }
    })()
} catch (e) {}

/* Don't assert or segfault. Tests PIC resetting. */
