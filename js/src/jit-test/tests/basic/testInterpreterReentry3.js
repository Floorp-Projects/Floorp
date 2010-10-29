function testInterpreterReentry3() {
    for (let i=0;i<5;++i) this["y" + i] = function(){};
    this.__defineGetter__('e', function (x2) { yield; });
    [1 for each (a in this) for (b in {})];
    return 1;
}
assertEq(testInterpreterReentry3(), 1);
