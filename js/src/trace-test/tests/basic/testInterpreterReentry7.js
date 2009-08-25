function testInterpreterReentry7() {
    var arr = [0, 1, 2, 3, 4];
    arr.__defineSetter__("4", function(x) { this._4 = x; });
    for (var i = 0; i < 5; i++)
        arr[i] = "grue";
    var tmp = arr._4;
    for (var p in arr)
        arr[p] = "bleen";
    return tmp + " " + arr._4;
}
assertEq(testInterpreterReentry7(), "grue bleen");
