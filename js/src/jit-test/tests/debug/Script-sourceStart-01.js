/*
 * Script.prototype.sourceStart and Script.prototype.sourceLength should both be
 * a number.
 */
let g = newGlobal('new-compartment');
let dbg = new Debugger(g);

var count = 0;
function test(string, range) {
    dbg.onNewScript = function (script) {
        ++count;
        assertEq(script.sourceStart, range[0]);
        assertEq(script.sourceLength, range[1]);
    };

    g.eval(string);
};

test("", [0, 0]);
test("2 * 3", [0, 5]);
test("2\n*\n3", [0, 5]);
assertEq(count, 3);
