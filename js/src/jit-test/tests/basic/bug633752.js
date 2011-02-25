function f(o) {
    var p = "arguments";
    for(var i=0; i<10; i++) {
        f[p];
    }
}
f({});
f({});
f({});
f({});

