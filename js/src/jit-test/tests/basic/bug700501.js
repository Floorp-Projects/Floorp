Function.prototype.__proto__["p"] = 3
c = [].__proto__
c[5] = 3
Namespace.prototype.__proto__[4] = function() {}
gc()
Function("\
    {\
    function f(d) {}\
    for each(let z in[0]) {\
        f(z)\
    }\
    }\
")()
