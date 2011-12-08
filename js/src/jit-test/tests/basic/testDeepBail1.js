function testDeepBail1() {
    var y = <z/>;
    for (var i = 0; i < 9; i++)
        "" in y;
}
testDeepBail1();
