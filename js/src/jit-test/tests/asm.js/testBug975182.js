Function("\
    g = (function(t,foreign){\
        \"use asm\";\
        var ff = foreign.ff;\
        function f() {\
            +ff()\
        }\
        return f\
    })(this, {\
        ff: arguments.callee\
    }, ArrayBuffer(4096))\
")()
function m(f) {
    for (var j = 0; j < 6000; ++j) {
        f();
    }
}
m(g);
