// Check evalInFrame("this") always returns the same object for a given frame.
// Primitive this-values should not be boxed multiple times.

var g = newGlobal();
var dbg = new Debugger(g);
var framesEntered = 0;
var framesPopped = 0;
var numSteps = 0;
dbg.onEnterFrame = function (frame) {
    if (frame.type === 'eval')
        return;
    framesEntered++;

    var frameThis = frame.eval('this').return;

    frame.onPop = function() {
        framesPopped++;
        assertEq(frame.eval('this').return, frameThis);
    };

    frame.onStep = function() {
        numSteps++;
        assertEq(frame.eval('this').return, frameThis);
    }

    g.gotThis = frameThis.unsafeDereference();
    assertEq(frame.this, frameThis);
};

g.eval("function nonstrictfun() { return this; }");
g.eval("nonstrictfun.call(Math); assertEq(gotThis, Math);");
g.eval("nonstrictfun.call(true); assertEq(gotThis.valueOf(), true);");
g.eval("nonstrictfun.call(); assertEq(gotThis, this);");

g.eval("function nonstrictfunNoThis() { return 1; }");
g.eval("nonstrictfunNoThis.call(Math); assertEq(gotThis, Math);");
g.eval("nonstrictfunNoThis.call(true); assertEq(gotThis.valueOf(), true);");
g.eval("nonstrictfunNoThis.call(); assertEq(gotThis, this);");

assertEq(framesEntered, 6);
assertEq(framesPopped, 6);
assertEq(numSteps > 15, true);
