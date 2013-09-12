// Exercise the call to ScriptDebugPrologue in js_InternalInterpret.

// This may change, but as of this writing, inline caches (ICs) are
// disabled in debug mode, and those are the only users of the out-of-line entry
// points for JIT code (arityCheckEntry, argsCheckEntry, fastEntry); debug
// mode uses only invokeEntry. This means most of the bytecode tails in
// js_InternalInterpret that might call ScriptPrologue or ScriptEpilogue are
// unreachable in debug mode: they're only called from the out-of-line entry
// points.
//
// The exception is REJOIN_THIS_PROTOTYPE, which can be reached reliably if you
// add a JS_GC call to stubs::GetPropNoCache. JIT code calls that stub to
// retrieve the 'prototype' property of a function called as a constructor, if
// TI can't establish the exact identity of that prototype's value at compile
// time. Thus the preoccupation with constructors here.

load(libdir + "asserts.js");

var debuggee = newGlobal();
var dbg = Debugger(debuggee);
var hits, savedFrame;

// Allow the constructor to return normally.
dbg.onEnterFrame = function (frame) {
    hits++;
    if (frame.constructing) {
        savedFrame = frame;
        assertEq(savedFrame.live, true);
        return undefined;
    }
    return undefined;
};
hits = 0;
debuggee.hits = 0;
savedFrame = undefined;
assertEq(typeof debuggee.eval("function f(){ hits++; } f.prototype = {}; new f;"), "object");
assertEq(hits, 2);
assertEq(savedFrame.live, false);
assertEq(debuggee.hits, 1);

// Force an early return from the constructor.
dbg.onEnterFrame = function (frame) {
    hits++;
    if (frame.constructing) {
        savedFrame = frame;
        assertEq(savedFrame.live, true);
        return { return: "pass" };
    }
    return undefined;
};
hits = 0;
debuggee.hits = 0;
savedFrame = undefined;
assertEq(typeof debuggee.eval("function f(){ hits++; } f.prototype = {}; new f;"), "object");
assertEq(hits, 2);
assertEq(savedFrame.live, false);
assertEq(debuggee.hits, 0);

// Force the constructor to throw an exception.
dbg.onEnterFrame = function (frame) {
    hits++;
    if (frame.constructing) {
        savedFrame = frame;
        assertEq(savedFrame.live, true);
        return { throw: "pass" };
    }
    return undefined;
};
hits = 0;
debuggee.hits = 0;
savedFrame = undefined;
assertThrowsValue(function () {
                      debuggee.eval("function f(){ hits++ } f.prototype = {}; new f;");
                  }, "pass");
assertEq(hits, 2);
assertEq(savedFrame.live, false);
assertEq(debuggee.hits, 0);

// Ensure that forcing an early return only returns from one JS call.
debuggee.eval("function g() { var result = new f; g_hits++; return result; }");
dbg.onEnterFrame = function (frame) {
    hits++;
    if (frame.constructing) {
        savedFrame = frame;
        assertEq(savedFrame.live, true);
        return { return: "pass" };
    }
    return undefined;
};
hits = 0;
debuggee.hits = 0;
debuggee.g_hits = 0;
savedFrame = undefined;
assertEq(typeof debuggee.eval("g();"), "object");
assertEq(hits, 3);
assertEq(savedFrame.live, false);
assertEq(debuggee.hits, 0);
assertEq(debuggee.g_hits, 1);
