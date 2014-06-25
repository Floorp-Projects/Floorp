/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * jorendb - A toy command-line debugger for shell-js programs.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * jorendb is a simple command-line debugger for shell-js programs. It is
 * intended as a demo of the Debugger object (as there are no shell js programs
 * to speak of).
 *
 * To run it: $JS -d path/to/this/file/jorendb.js
 * To run some JS code under it, try:
 *    (jorendb) print load("my-script-to-debug.js")
 * Execution will stop at debugger statements and you'll get a jorendb prompt.
 */

// Debugger state.
var focusedFrame = null;
var topFrame = null;
var debuggeeValues = {};
var nextDebuggeeValueIndex = 1;
var lastExc = null;

// Cleanup functions to run when we next re-enter the repl.
var replCleanups = [];

// Convert a debuggee value v to a string.
function dvToString(v) {
    return (typeof v !== 'object' || v === null) ? uneval(v) : "[object " + v.class + "]";
}

function showDebuggeeValue(dv) {
    var dvrepr = dvToString(dv);
    var i = nextDebuggeeValueIndex++;
    debuggeeValues["$" + i] = dv;
    print("$" + i + " = " + dvrepr);
}

Object.defineProperty(Debugger.Frame.prototype, "num", {
    configurable: true,
    enumerable: false,
    get: function () {
            var i = 0;
            for (var f = topFrame; f && f !== this; f = f.older)
                i++;
            return f === null ? undefined : i;
        }
    });

Debugger.Frame.prototype.frameDescription = function frameDescription() {
    if (this.type == "call")
        return ((this.callee.name || '<anonymous>') +
                "(" + this.arguments.map(dvToString).join(", ") + ")");
    else
        return this.type + " code";
}

Debugger.Frame.prototype.positionDescription = function positionDescription() {
    if (this.script) {
        var line = this.script.getOffsetLine(this.offset);
        if (this.script.url)
            return this.script.url + ":" + line;
        return "line " + line;
    }
    return null;
}

Debugger.Frame.prototype.fullDescription = function fullDescription() {
    var fr = this.frameDescription();
    var pos = this.positionDescription();
    if (pos)
        return fr + ", " + pos;
    return fr;
}

Object.defineProperty(Debugger.Frame.prototype, "line", {
        configurable: true,
        enumerable: false,
        get: function() {
            if (this.script)
                return this.script.getOffsetLine(this.offset);
            else
                return null;
        }
    });

function callDescription(f) {
    return ((f.callee.name || '<anonymous>') +
            "(" + f.arguments.map(dvToString).join(", ") + ")");
}

function showFrame(f, n) {
    if (f === undefined || f === null) {
        f = focusedFrame;
        if (f === null) {
            print("No stack.");
            return;
        }
    }
    if (n === undefined) {
        n = f.num;
        if (n === undefined)
            throw new Error("Internal error: frame not on stack");
    }

    print('#' + n + " " + f.fullDescription());
}

function saveExcursion(fn) {
    var tf = topFrame, ff = focusedFrame;
    try {
        return fn();
    } finally {
        topFrame = tf;
        focusedFrame = ff;
    }
}

// Evaluate an expression in the Debugger global
function evalCommand(expr) {
    eval(expr);
}

function quitCommand() {
    dbg.enabled = false;
    quit(0);
}

function backtraceCommand() {
    if (topFrame === null)
        print("No stack.");
    for (var i = 0, f = topFrame; f; i++, f = f.older)
        showFrame(f, i);
}

function printCommand(rest) {
    // This is the real deal.
    var cv = saveExcursion(
        () => focusedFrame == null
              ? debuggeeGlobalWrapper.evalInGlobalWithBindings(rest, debuggeeValues)
              : focusedFrame.evalWithBindings(rest, debuggeeValues));
    if (cv === null) {
        if (!dbg.enabled)
            return [cv];
        print("Debuggee died.");
    } else if ('return' in cv) {
        if (!dbg.enabled)
            return [undefined];
        showDebuggeeValue(cv.return);
    } else {
        if (!dbg.enabled)
            return [cv];
        print("Exception caught. (To rethrow it, type 'throw'.)");
        lastExc = cv.throw;
        showDebuggeeValue(lastExc);
    }
}

function detachCommand() {
    dbg.enabled = false;
    return [undefined];
}

function continueCommand() {
    if (focusedFrame === null) {
        print("No stack.");
        return;
    }
    return [undefined];
}

function throwCommand(rest) {
    var v;
    if (focusedFrame !== topFrame) {
        print("To throw, you must select the newest frame (use 'frame 0').");
        return;
    } else if (focusedFrame === null) {
        print("No stack.");
        return;
    } else if (rest === '') {
        return [{throw: lastExc}];
    } else {
        var cv = saveExcursion(function () { return focusedFrame.eval(rest); });
        if (cv === null) {
            if (!dbg.enabled)
                return [cv];
            print("Debuggee died while determining what to throw. Stopped.");
        } else if ('return' in cv) {
            return [{throw: cv.return}];
        } else {
            if (!dbg.enabled)
                return [cv];
            print("Exception determining what to throw. Stopped.");
            showDebuggeeValue(cv.throw);
        }
        return;
    }
}

function frameCommand(rest) {
    var n, f;
    if (rest.match(/[0-9]+/)) {
        n = +rest;
        f = topFrame;
        if (f === null) {
            print("No stack.");
            return;
        }
        for (var i = 0; i < n && f; i++) {
            if (!f.older) {
                print("There is no frame " + rest + ".");
                return;
            }
            f.older.younger = f;
            f = f.older;
        }
        focusedFrame = f;
        showFrame(f, n);
    } else if (rest !== '') {
        if (topFrame === null)
            print("No stack.");
        else
            showFrame();
    } else {
        print("do what now?");
    }
}

function upCommand() {
    if (focusedFrame === null)
        print("No stack.");
    else if (focusedFrame.older === null)
        print("Initial frame selected; you cannot go up.");
    else {
        focusedFrame.older.younger = focusedFrame;
        focusedFrame = focusedFrame.older;
        showFrame();
    }
}

function downCommand() {
    if (focusedFrame === null)
        print("No stack.");
    else if (!focusedFrame.younger)
        print("Youngest frame selected; you cannot go down.");
    else {
        focusedFrame = focusedFrame.younger;
        showFrame();
    }
}

function forcereturnCommand(rest) {
    var v;
    var f = focusedFrame;
    if (f !== topFrame) {
        print("To forcereturn, you must select the newest frame (use 'frame 0').");
    } else if (f === null) {
        print("Nothing on the stack.");
    } else if (rest === '') {
        return [{return: undefined}];
    } else {
        var cv = saveExcursion(function () { return f.eval(rest); });
        if (cv === null) {
            if (!dbg.enabled)
                return [cv];
            print("Debuggee died while determining what to forcereturn. Stopped.");
        } else if ('return' in cv) {
            return [{return: cv.return}];
        } else {
            if (!dbg.enabled)
                return [cv];
            print("Error determining what to forcereturn. Stopped.");
            showDebuggeeValue(cv.throw);
        }
    }
}

function printPop(f, c) {
    var fdesc = f.fullDescription();
    if (c.return) {
        print("frame returning (still selected): " + fdesc);
        showDebuggeeValue(c.return);
    } else if (c.throw) {
        print("frame threw exception: " + fdesc);
        showDebuggeeValue(c.throw);
        print("(To rethrow it, type 'throw'.)");
        lastExc = c.throw;
    } else {
        print("frame was terminated: " + fdesc);
    }
}

// Set |prop| on |obj| to |value|, but then restore its current value
// when we next enter the repl.
function setUntilRepl(obj, prop, value) {
    var saved = obj[prop];
    obj[prop] = value;
    replCleanups.push(function () { obj[prop] = saved; });
}

function doStepOrNext(kind) {
    var startFrame = topFrame;
    var startLine = startFrame.line;
    print("stepping in:   " + startFrame.fullDescription());
    print("starting line: " + uneval(startLine));

    function stepPopped(completion) {
        // Note that we're popping this frame; we need to watch for
        // subsequent step events on its caller.
        this.reportedPop = true;
        printPop(this, completion);
        topFrame = focusedFrame = this;
        return repl();
    }

    function stepEntered(newFrame) {
        print("entered frame: " + newFrame.fullDescription());
        topFrame = focusedFrame = newFrame;
        return repl();
    }

    function stepStepped() {
        print("stepStepped: " + this.fullDescription());
        // If we've changed frame or line, then report that.
        if (this !== startFrame || this.line != startLine) {
            topFrame = focusedFrame = this;
            if (focusedFrame != startFrame)
                print(focusedFrame.fullDescription());
            return repl();
        }

        // Otherwise, let execution continue.
        return undefined;
    }

    if (kind.step)
        setUntilRepl(dbg, 'onEnterFrame', stepEntered);

    // If we're stepping after an onPop, watch for steps and pops in the
    // next-older frame; this one is done.
    var stepFrame = startFrame.reportedPop ? startFrame.older : startFrame;
    if (!stepFrame || !stepFrame.script)
        stepFrame = null;
    if (stepFrame) {
        setUntilRepl(stepFrame, 'onStep', stepStepped);
        setUntilRepl(stepFrame, 'onPop',  stepPopped);
    }

    // Let the program continue!
    return [undefined];
}

function stepCommand() { return doStepOrNext({step:true}); }
function nextCommand() { return doStepOrNext({next:true}); }

// Build the table of commands.
var commands = {};
var commandArray = [
    backtraceCommand, "bt", "where",
    continueCommand, "c",
    detachCommand,
    downCommand, "d",
    forcereturnCommand,
    frameCommand, "f",
    nextCommand, "n",
    printCommand, "p",
    quitCommand, "q",
    stepCommand, "s",
    throwCommand, "t",
    upCommand, "u",
    helpCommand, "h",
    evalCommand, "!",
    ];
var last = null;
for (var i = 0; i < commandArray.length; i++) {
    var cmd = commandArray[i];
    if (typeof cmd === "string")
        commands[cmd] = last;
    else
        last = commands[cmd.name.replace(/Command$/, '')] = cmd;
}

function helpCommand(rest) {
    print("Available commands:");
    var printcmd = function(group) {
        print("  " + group.join(", "));
    }

    var group = [];
    for (var cmd of commandArray) {
        if (typeof cmd === "string") {
            group.push(cmd);
        } else {
            if (group.length) printcmd(group);
            group = [ cmd.name.replace(/Command$/, '') ];
        }
    }
    printcmd(group);
}

// Break cmd into two parts: its first word and everything else. If it begins
// with punctuation, treat that as a separate word.
function breakcmd(cmd) {
    cmd = cmd.trimLeft();
    if ("!@#$%^&*_+=/?.,<>:;'\"".indexOf(cmd.substr(0, 1)) != -1)
        return [cmd.substr(0, 1), cmd.substr(1).trimLeft()];
    var m = /\s/.exec(cmd);
    if (m === null)
        return [cmd, ''];
    return [cmd.slice(0, m.index), cmd.slice(m.index).trimLeft()];
}

function runcmd(cmd) {
    var pieces = breakcmd(cmd);
    if (pieces[0] === "")
        return undefined;

    var first = pieces[0], rest = pieces[1];
    if (!commands.hasOwnProperty(first)) {
        print("unrecognized command '" + first + "'");
        return undefined;
    }

    var cmd = commands[first];
    if (cmd.length === 0 && rest !== '') {
        print("this command cannot take an argument");
        return undefined;
    }

    return cmd(rest);
}

function repl() {
    while (replCleanups.length > 0)
        replCleanups.pop()();

    var cmd;
    for (;;) {
        putstr("\n" + prompt);
        cmd = readline();
        if (cmd === null)
            return null;

        try {
            var result = runcmd(cmd);
            if (result === undefined)
                ; // do nothing
            else if (Array.isArray(result))
                return result[0];
            else
                throw new Error("Internal error: result of runcmd wasn't array or undefined");
        } catch (exc) {
            print("*** Internal error: exception in the debugger code.");
            print("    " + exc);
            print(exc.stack);
        }
    }
}

var dbg = new Debugger();
dbg.onDebuggerStatement = function (frame) {
    return saveExcursion(function () {
            topFrame = focusedFrame = frame;
            print("'debugger' statement hit.");
            showFrame();
            return repl();
        });
};
dbg.onThrow = function (frame, exc) {
    return saveExcursion(function () {
            topFrame = focusedFrame = frame;
            print("Unwinding due to exception. (Type 'c' to continue unwinding.)");
            showFrame();
            print("Exception value is:");
            showDebuggeeValue(exc);
            return repl();
        });
};

// The depth of jorendb nesting.
var jorendbDepth;
if (typeof jorendbDepth == 'undefined') jorendbDepth = 0;

var debuggeeGlobal = newGlobal("new-compartment");
debuggeeGlobal.jorendbDepth = jorendbDepth + 1;
var debuggeeGlobalWrapper = dbg.addDebuggee(debuggeeGlobal);

print("jorendb version -0.0");
prompt = '(' + Array(jorendbDepth+1).join('meta-') + 'jorendb) ';

var args = arguments;
while(args.length > 0) {
    var arg = args.shift();
    if (arg == '-f') {
        arg = args.shift();
        debuggeeGlobal.evaluate(read(arg), { fileName: arg, lineNumber: 1 });
    } else if (arg == '-e') {
        arg = args.shift();
        debuggeeGlobal.eval(arg);
    } else {
        throw("jorendb does not implement command-line argument '" + arg + "'");
    }
}

repl();
