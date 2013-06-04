/*
 * For function statements, Script.prototype.sourceStart and
 * Script.prototype.sourceLength should comprise both the opening '(' and the
 * closing '}'.
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

test("function f() {}", [[10, 5]]);
test("function f() { function g() {} }", [[10, 22], [25, 5]]);
test("function f() { function g() { function h() {} } }", [[10, 39], [25, 22], [40, 5]]);
test("function f() { if (true) function g() {} }", [[10, 32], [35, 5]]); 
test("var o = { get p () {} }", [[16, 5]]);
test("var o = { set p (x) {} }", [[16, 6]]);
