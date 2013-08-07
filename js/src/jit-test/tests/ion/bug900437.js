function test() {
    var x = 0.0;
    for (var i = 0; i < 100; i++)
        -("") >> (x / x);
}
test();
