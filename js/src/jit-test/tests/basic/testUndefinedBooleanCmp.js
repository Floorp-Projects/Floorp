function testUndefinedBooleanCmp()
{
    var t = true, f = false, x = [];
    for (var i = 0; i < 10; ++i) {
        x[0] = t == undefined;
        x[1] = t != undefined;
        x[2] = t === undefined;
        x[3] = t !== undefined;
        x[4] = t < undefined;
        x[5] = t > undefined;
        x[6] = t <= undefined;
        x[7] = t >= undefined;
        x[8] = f == undefined;
        x[9] = f != undefined;
        x[10] = f === undefined;
        x[11] = f !== undefined;
        x[12] = f < undefined;
        x[13] = f > undefined;
        x[14] = f <= undefined;
        x[15] = f >= undefined;
    }
    return x.join(",");
}
assertEq(testUndefinedBooleanCmp(), "false,true,false,true,false,false,false,false,false,true,false,true,false,false,false,false");
