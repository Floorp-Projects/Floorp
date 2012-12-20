var g = newGlobal('new-compartment');
var dbg = Debugger(g);
function test(code, expected) {
    var actual = '';
    g.h = function () { actual += dbg.getNewestFrame().environment.type; }
    g.eval(code);
}
test("h();", 'object');
gczeal(4);
var count2 = countHeap();
