function testMethodInit() {
    function o() {}
    function k() {
        for (i = 0; i < this.depth; ++i) {}
    }
    for (var i = 0; i < 10; i++)
        (i) = {o: o, k: k};
}
testMethodInit();
