// |jit-test| error:SyntaxError

(function () {
    for (a in (function () {
        yield Array.reduce()
    })()) function () {}
})()
