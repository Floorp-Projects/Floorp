var obj = new Object();
var passed = true;
for (var i = 0; i < 100; i++) {
    if (obj['-1'] == null)
        obj['-1'] = new Array();
    assertEq(obj['-1'] == null, false);
    obj = new Object();
}
