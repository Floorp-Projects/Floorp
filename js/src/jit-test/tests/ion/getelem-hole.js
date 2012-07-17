var a = [1, 2, 3];
var b = [4, 5, 6];

function testFold() {
    for (var i=0; i<10; i++) {
        var x = a[i];
        var y = a[i];
        var z = b[i];
        assertEq(x, y);
        if (i < 3)
            assertEq(x !== z, true);
    }
}
for (var i=0; i<10; i++)
    testFold();
