// Don't crash or assert.

this.watch("d", eval);
(function () {
    (eval("\
    (function () {\
        for (let x = 0; x < 2; ++x) {\
            d = x\
        }\
    })\
"))()
})()
