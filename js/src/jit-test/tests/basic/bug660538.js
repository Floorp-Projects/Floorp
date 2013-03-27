Error.prototype.__proto__.p = 5;
f = Function("return( \"\" <arguments for(w in[]))");
for (i in f()) {}

(function() {
    for (a in (
        arguments for (l in [0])
    ))
    {}
})()
