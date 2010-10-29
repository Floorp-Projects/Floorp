function loopWithUndefined1(t, val) {
    var a = new Array(6);
    for (var i = 0; i < 6; i++)
        a[i] = (t > val);
    return a;
}
loopWithUndefined1(5.0, 2);     //compile version with val=int

function testLoopWithUndefined1() {
    return loopWithUndefined1(5.0).join(",");  //val=undefined
};
assertEq(testLoopWithUndefined1(), "false,false,false,false,false,false");
