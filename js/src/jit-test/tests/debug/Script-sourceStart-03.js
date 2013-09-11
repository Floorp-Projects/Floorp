/*
 * For arrow functions, Script.prototype.sourceStart and
 * Script.prototype.sourceLength should comprise the entire function expression
 * (including arguments)
 */
let g = newGlobal();
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

    /*
     * In some configurations certain child scripts are listed twice, so we
     * cannot rely on index always having the exact same value
     */
    assertEq(0 < index && index <= ranges.length, true);
};

test("() => {}", [[0, 8]]);
test("(x, y) => { x * y }", [[0, 19]]);
test("x => x * x", [[0, 10]]);
test("x => x => x * x", [[0, 15], [5, 10], [5, 10]]);
