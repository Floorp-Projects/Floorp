// |jit-test| need-for-each

function testThinForEach() {
    var a = ["red"];
    var n = 0;
    for (var i = 0; i < 10; i++)
        for each (var v in a)
            if (v)
                n++;
    return n;
}
assertEq(testThinForEach(), 10);
