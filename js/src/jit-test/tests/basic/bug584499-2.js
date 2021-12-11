function f(x) {
    var s = "a";
    var last = "";
    for (var i = 0; i < 10; i++) {
        last = s[x];
    }
    return last;
}

f(0);

assertEq(f(1), undefined);
