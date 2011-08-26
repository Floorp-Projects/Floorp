// Throwing and catching an error in an onStep handler shouldn't interfere
// with throwing and catching in the debuggee.

var g = newGlobal('new-compartment');
g.eval("function f() { debugger; throw 'mud'; }");

var dbg = Debugger(g);
var stepped = false;
dbg.onDebuggerStatement = function (frame) {
    frame.older.onStep = function () {
        stepped = true;
        try {
            throw 'snow';
        } catch (x) {
            assertEq(x, 'snow');
        }
    };
};

stepped = false;
g.eval("var caught;\n" +
       "try {\n" +
       "    f();\n" +
       "} catch (x) {\n" +
       "    caught = x;\n" +
       "}\n");
assertEq(stepped, true);
assertEq(g.caught, 'mud');
