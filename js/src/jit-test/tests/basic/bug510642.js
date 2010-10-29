// Don't crash or assert.

(function () {
    var x;
    (eval("\
    (function () {\
        for (y in [0, 0]) let(a)((function () {\
            for (w in [0, 0])\
                x = 0\
        })());\
        with({}) {}\
    })\
    "))()
})()
