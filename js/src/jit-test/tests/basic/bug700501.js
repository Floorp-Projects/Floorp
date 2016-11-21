// |jit-test| need-for-each

Function.prototype.__proto__["p"] = 3
c = [].__proto__
c[5] = 3
gc()
Function("\
    {\
    function f(d) {}\
    for each(let z in[0]) {\
        f(z)\
    }\
    }\
")()
