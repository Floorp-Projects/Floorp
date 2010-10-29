function testDeepBail1() {
    var y = <z/>;
    for (var i = 0; i < RUNLOOP; i++)
        "" in y;
}
testDeepBail1();
