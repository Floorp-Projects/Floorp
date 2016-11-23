/*
 * For eval and Function constructors, Script.prototype.sourceStart and
 * Script.prototype.sourceLength should comprise the entire script (excluding
 * arguments in the case of Function constructors)
 */
let g = newGlobal();
let dbg = new Debugger(g);

var count = 0;
function test(string, range) {
    dbg.onNewScript = function (script) {
        ++count;
        if (count % 2 == 0) {
            assertEq(script.sourceStart, range[0]);
            assertEq(script.sourceLength, range[1]);
        }
    }

    g.eval(string);
}

test("eval('2 * 3')", [0, 5]);
test("new Function('2 * 3')", [0, 12]);
test("new Function('x', 'x * x')", [0, 13]);
assertEq(count, 6);
