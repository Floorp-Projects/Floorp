function g(f) {
    for (var j = 0; j < 9; ++j) {
        try {
            f()
        } catch (e) {}
    }
}
function h(code) {
    Function(code)();
}
g([]);
g([]);
g([]);
g([]);
g([]);
g([]);
g([]);
g([]);
g([]);
g([]);
g([]);
g([]);
g([]);
g([]);
g([]);
g([]);
g([]);
g([]);
g([]);
g([]);
g([]);
g([]);
g([]);
g([]);
g([]);
g([]);
g([]);
g([]);
g([]);
g([]);
h("\
    m = (function(stdlib, foreign) { \
        \"use asm\";\
        var ff=foreign.ff;\
        function f(){\
            ff(0);\
        } \
        return f \
    })(this , { \
        ff: arguments.callee.caller\
    });\
    g(m , []);\
");
h("\
    m = undefined;\
    gc();\
")
