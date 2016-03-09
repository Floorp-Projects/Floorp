// Ensure |Frame.this| returns the right value even if we're still in the
// script's prologue, before JSOP_FUNCTIONTHIS.

var g = newGlobal();
var dbg = new Debugger(g);
var hits = 0;
dbg.onEnterFrame = function (frame) {
    if (frame.type === 'eval')
        return;
    hits++;

    var frameThis = frame.this;
    if (frameThis !== null && typeof frameThis === "object")
        g.gotThis = frameThis.unsafeDereference();
    else
        g.gotThis = frameThis;

    assertEq(frame.this, frameThis, "getter should not return a different object");
};

// Strict mode, function uses |this|.
g.eval("function strictfun() { 'use strict'; return this; }");
g.eval("strictfun.call(Math); assertEq(gotThis, Math);");
g.eval("strictfun.call(true); assertEq(gotThis, true);");
g.eval("strictfun.call();     assertEq(gotThis, undefined);");

// Strict mode, function doesn't use |this|.
g.eval("function strictfunNoThis() { 'use strict'; }");
g.eval("strictfunNoThis.call(Math); assertEq(gotThis, Math);");
g.eval("strictfunNoThis.call(true); assertEq(gotThis, true);");
g.eval("strictfunNoThis.call(null); assertEq(gotThis, null);");

// Non-strict mode (primitive |this| is boxed), function uses |this|.
g.eval("function nonstrictfun() { return this; }");
g.eval("nonstrictfun.call(Math); assertEq(gotThis, Math);");
g.eval("nonstrictfun.call(null); assertEq(gotThis, this);");
g.eval("nonstrictfun.call();     assertEq(gotThis, this);");

// Non-strict mode (primitive |this| is boxed), function doesn't use |this|.
g.eval("function nonstrictfunNoThis() {}");
g.eval("nonstrictfunNoThis.call(Math); assertEq(gotThis, Math);");
g.eval("nonstrictfunNoThis.call(null); assertEq(gotThis, this);");
g.eval("nonstrictfunNoThis.call();     assertEq(gotThis, this);");

assertEq(hits, 12);
