function testInterpreterReentry5() {
    var arr = [0, 1, 2, 3, 4];
    arr.__defineGetter__("4", function() { return 1000; });
    for (var i = 0; i < 5; i++)
        arr[i];
    for (var p in arr)
        arr[p];
}
testInterpreterReentry5();
