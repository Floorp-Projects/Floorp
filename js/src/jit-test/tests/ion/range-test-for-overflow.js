function f() {
    var i;
    for (i = 8; i <2147483647; i+=10);
    print(i);
}

for (var i = 0; i < 60; i++) {
    f();
}
