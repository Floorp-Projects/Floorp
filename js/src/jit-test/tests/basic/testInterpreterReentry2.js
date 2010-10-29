function testInterpreterReentry2() {
    var a = false;
    var b = {};
    var c = false;
    var d = {};
    this.__defineGetter__('e', function(){});
    for (let f in this) print(f);
    [1 for each (g in this) for each (h in [])]
    return 1;
}
assertEq(testInterpreterReentry2(), 1);
