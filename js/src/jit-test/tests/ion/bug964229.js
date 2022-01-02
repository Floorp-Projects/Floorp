a = 'a';
b = 0
var i=0;
exhaustiveSliceTest("exhaustive slice test 1", a);
var i=1;
exhaustiveSliceTest("exhaustive slice test 2", b);
exhaustiveSliceTest("exhaustive slice test 3", 0);
var i=0;
var executed = false;
try {
    exhaustiveSliceTest("exhaustive slice test 4", 0);
} catch(e) {
    executed = true;
}
assertEq(executed, true);

function exhaustiveSliceTest(testname, a) {
    print(testname)
    for (var y = 0; y < 2; y++)
    {
        print(a.length)
        if (a.length == 2 || i == 1)
            return 0;
        var b  = a.slice(0,0);
    }
}
