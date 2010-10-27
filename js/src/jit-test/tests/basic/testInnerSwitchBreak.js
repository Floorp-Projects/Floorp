function innerSwitch(k)
{
    var m = 0;

    switch (k)
    {
    case 0:
        m = 1;
        break;
    }

    return m;
}
function testInnerSwitchBreak()
{
    var r = new Array(5);
    for (var i = 0; i < 5; i++)
    {
        r[i] = innerSwitch(0);
    }

    return r.join(",");
}
assertEq(testInnerSwitchBreak(), "1,1,1,1,1");
