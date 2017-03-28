function test() {
    var o = {};
    o.watch('x', test);
    try {
        o.x = 3;
    } catch(e) {}
}
test();
