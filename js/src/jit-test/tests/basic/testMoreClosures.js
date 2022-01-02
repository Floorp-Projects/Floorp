function testMoreClosures() {
    var f = {}, max = 3;

    var hello = function(n) {
        function howdy() { return n * n }
        f.test = howdy;
    };

    for (var i = 0; i <= max; i++)
        hello(i);

    return f.test();
}
assertEq(testMoreClosures(), 9);
