function g(s) {
    for (var i = s; ; i--) {
        if (s[i] !== ' ')
            return;
    }
}
function f() {
    var s = "foo".match(/(.*)$/)[0];
    return g(s);
}
for (var i = 0; i < 10; i++)
    f();
