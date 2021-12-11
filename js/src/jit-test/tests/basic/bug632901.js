// don't crash when tracing
function f(o) {
    var prop = "arguments";
    f[prop] = f[prop];
}
for(var i = 0; i < 50; i++) {
    f();
}

