function testInterpreterReentry2() {
    var a = false;
    var b = {};
    var c = false;
    var d = {};
    this.__defineGetter__('e', function(){});
    for (let f in this) print(f);
    return 1;
}
assertEq(testInterpreterReentry2(), 1);
