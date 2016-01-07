function testInterpreterReentry3() {
    for (let i=0;i<5;++i) this["y" + i] = function(){};
    this.__defineGetter__('e', function (x2) { yield; });
    return 1;
}
assertEq(testInterpreterReentry3(), 1);
