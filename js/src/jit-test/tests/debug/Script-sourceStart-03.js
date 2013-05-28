/*
 * For arrow functions, Script.prototype.sourceStart and
 * Script.prototype.sourceLength should comprise the entire function expression
 * (including arguments)
 */
let g = newGlobal('new-compartment');
let dbg = new Debugger(g);

function test(string, ranges) {
    var index = 0;
    dbg.onNewScript = function (script) {
        function traverse(script) {
            script.getChildScripts().forEach(function (script) {
                assertEq(script.sourceStart, ranges[index][0]);
                assertEq(script.sourceLength, ranges[index][1]);
                ++index;
                traverse(script);
            });
        }
        traverse(script);
    };

    g.eval(string);
    assertEq(index, ranges.length);
};

test("() => {}", [[0, 8]]);
test("(x, y) => { x * y }", [[0, 19]]);
test("x => x * x", [[0, 10]]);
test("x => x => x * x", [[0, 15], [5, 10]]);
test("x => x => x => x * x", [[0, 20], [5, 15], [10, 10]]);
