function testDeepBail1() {
    var y = [1,2,3];
    for (var i = 0; i < 9; i++)
        "" in y;
}
testDeepBail1();
