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
var todo = [];
var activeTask;
var options = { 'pretty': true,
                'emacs': (os.getenv('EMACS') == 't') };
var rerun = true;

// Cleanup functions to run when we next re-enter the repl.
var replCleanups = [];

// Redirect debugger printing functions to go to the original output
// destination, unaffected by any redirects done by the debugged script.
var initialOut = os.file.redirect();
var initialErr = os.file.redirectErr();

function wrap(global, name) {
    var orig = global[name];
    global[name] = function(...args) {

        var oldOut = os.file.redirect(initialOut);
        var oldErr = os.file.redirectErr(initialErr);
        try {
            return orig.apply(global, args);
        } finally {
            os.file.redirect(oldOut);
            os.file.redirectErr(oldErr);
        }
    };
}
wrap(this, 'print');
wrap(this, 'printErr');
wrap(this, 'putstr');

// Convert a debuggee value v to a string.
function dvToString(v) {
    return (typeof v !== 'object' || v === null) ? uneval(v) : "[object " + v.class + "]";
}

function summaryObject(dv) {
    var obj = {};
    for (var name of dv.getOwnPropertyNames()) {
        var v = dv.getOwnPropertyDescriptor(name).value;
        if (v instanceof Debugger.Object) {
            v = "(...)";
        }
        obj[name] = v;
    }
    return obj;
}

function debuggeeValueToString(dv, style) {
    var dvrepr = dvToString(dv);
    if (!style.pretty || (typeof dv !== 'object'))
        return [dvrepr, undefined];

    if (dv.class == "Error") {
        let errval = debuggeeGlobalWrapper.executeInGlobalWithBindings("$" + i + ".toString()", debuggeeValues);
        return [dvrepr, errval.return];
    }

    if (style.brief)
        return [dvrepr, JSON.stringify(summaryObject(dv), null, 4)];

    let str = debuggeeGlobalWrapper.executeInGlobalWithBindings("JSON.stringify(v, null, 4)", {v: dv});
    if ('throw' in str) {
        if (style.noerror)
            return [dvrepr, undefined];

        let substyle = {};
        Object.assign(substyle, style);
        substyle.noerror = true;
        return [dvrepr, debuggeeValueToString(str.throw, substyle)];
    }

    return [dvrepr, str.return];
}

// Problem! Used to do [object Object] followed by details. Now just details?

function showDebuggeeValue(dv, style={pretty: options.pretty}) {
    var i = nextDebuggeeValueIndex++;
    debuggeeValues["$" + i] = dv;
    let [brief, full] = debuggeeValueToString(dv, style);
    print("$" + i + " = " + brief);
    if (full !== undefined)
        print(full);
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
        var line = this.script.getOffsetLocation(this.offset).lineNumber;
        if (this.script.url)
            return this.script.url + ":" + line;
        return "line " + line;
    }
    return null;
}

Debugger.Frame.prototype.location = function () {
    if (this.script) {
        var { lineNumber, columnNumber, isEntryPoint } = this.script.getOffsetLocation(this.offset);
        if (this.script.url)
            return this.script.url + ":" + lineNumber;
        return null;
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
                return this.script.getOffsetLocation(this.offset).lineNumber;
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

function parseArgs(str) {
    return str.split(" ");
}

function describedRv(r, desc) {
    desc = "[" + desc + "] ";
    if (r === undefined) {
        print(desc + "Returning undefined");
    } else if (r === null) {
        print(desc + "Returning null");
    } else if (r.length === undefined) {
        print(desc + "Returning object " + JSON.stringify(r));
    } else {
        print(desc + "Returning length-" + r.length + " list");
        if (r.length > 0) {
            print("  " + r[0]);
        }
    }
    return r;
}

// Rerun the program (reloading it from the file)
function runCommand(args) {
    print("Restarting program");
    if (args)
        activeTask.scriptArgs = parseArgs(args);
    rerun = true;
    for (var f = topFrame; f; f = f.older) {
        if (f.older) {
            f.onPop = () => null;
        } else {
            f.onPop = () => ({ 'return': 0 });
        }
    }
    //return describedRv([{ 'return': 0 }], "runCommand");
    return null;
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

function setCommand(rest) {
    var space = rest.indexOf(' ');
    if (space == -1) {
        print("Invalid set <option> <value> command");
    } else {
        var name = rest.substr(0, space);
        var value = rest.substr(space + 1);

        if (name == 'args') {
            activeTask.scriptArgs = parseArgs(value);
        } else {
            var yes = ["1", "yes", "true", "on"];
            var no = ["0", "no", "false", "off"];

            if (yes.indexOf(value) !== -1)
                options[name] = true;
            else if (no.indexOf(value) !== -1)
                options[name] = false;
            else
                options[name] = value;
        }
    }
}

function split_print_options(s, style) {
    var m = /^\/(\w+)/.exec(s);
    if (!m)
        return [ s, style ];
    if (m[1].indexOf("p") != -1)
        style.pretty = true;
    if (m[1].indexOf("b") != -1)
        style.brief = true;
    return [ s.substr(m[0].length).trimLeft(), style ];
}

function doPrint(expr, style) {
    // This is the real deal.
    var cv = saveExcursion(
        () => focusedFrame == null
              ? debuggeeGlobalWrapper.executeInGlobalWithBindings(expr, debuggeeValues)
              : focusedFrame.evalWithBindings(expr, debuggeeValues));
    if (cv === null) {
        if (!dbg.enabled)
            return [cv];
        print("Debuggee died.");
    } else if ('return' in cv) {
        if (!dbg.enabled)
            return [undefined];
        showDebuggeeValue(cv.return, style);
    } else {
        if (!dbg.enabled)
            return [cv];
        print("Exception caught. (To rethrow it, type 'throw'.)");
        lastExc = cv.throw;
        showDebuggeeValue(lastExc, style);
    }
}

function printCommand(rest) {
    var [expr, style] = split_print_options(rest, {pretty: options.pretty});
    return doPrint(expr, style);
}

function keysCommand(rest) { return doPrint("Object.keys(" + rest + ")"); }

function detachCommand() {
    dbg.enabled = false;
    return [undefined];
}

function continueCommand(rest) {
    if (focusedFrame === null) {
        print("No stack.");
        return;
    }

    var match = rest.match(/^(\d+)$/);
    if (match) {
        return doStepOrNext({upto:true, stopLine:match[1]});
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
        updateLocation(focusedFrame);
        showFrame(f, n);
    } else if (rest === '') {
        if (topFrame === null) {
            print("No stack.");
        } else {
            updateLocation(focusedFrame);
            showFrame();
        }
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
        updateLocation(focusedFrame);
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
        updateLocation(focusedFrame);
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
        showDebuggeeValue(c.return, {brief: true});
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

function updateLocation(frame) {
    if (options.emacs) {
        var loc = frame.location();
        if (loc)
            print("\032\032" + loc + ":1");
    }
}

function doStepOrNext(kind) {
    var startFrame = topFrame;
    var startLine = startFrame.line;
    // print("stepping in:   " + startFrame.fullDescription());
    // print("starting line: " + uneval(startLine));

    function stepPopped(completion) {
        // Note that we're popping this frame; we need to watch for
        // subsequent step events on its caller.
        this.reportedPop = true;
        printPop(this, completion);
        topFrame = focusedFrame = this;
        if (kind.finish) {
            // We want to continue, but this frame is going to be invalid as
            // soon as this function returns, which will make the replCleanups
            // assert when it tries to access the dead frame's 'onPop'
            // property. So clear it out now while the frame is still valid,
            // and trade it for an 'onStep' callback on the frame we're popping to.
            preReplCleanups();
            setUntilRepl(this.older, 'onStep', stepStepped);
            return undefined;
        }
        updateLocation(this);
        return repl();
    }

    function stepEntered(newFrame) {
        print("entered frame: " + newFrame.fullDescription());
        updateLocation(newFrame);
        topFrame = focusedFrame = newFrame;
        return repl();
    }

    function stepStepped() {
        // print("stepStepped: " + this.fullDescription());
        updateLocation(this);
        var stop = false;

        if (kind.finish) {
            // 'finish' set a one-time onStep for stopping at the frame it
            // wants to return to
            stop = true;
        } else if (kind.upto) {
            // running until a given line is reached
            if (this.line == kind.stopLine)
                stop = true;
        } else {
            // regular step; stop whenever the line number changes
            if ((this.line != startLine) || (this != startFrame))
                stop = true;
        }

        if (stop) {
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
        if (!kind.finish)
            setUntilRepl(stepFrame, 'onStep', stepStepped);
        setUntilRepl(stepFrame, 'onPop',  stepPopped);
    }

    // Let the program continue!
    return [undefined];
}

function stepCommand() { return doStepOrNext({step:true}); }
function nextCommand() { return doStepOrNext({next:true}); }
function finishCommand() { return doStepOrNext({finish:true}); }

// FIXME: DOES NOT WORK YET
function breakpointCommand(where) {
    print("Sorry, breakpoints don't work yet.");
    var script = focusedFrame.script;
    var offsets = script.getLineOffsets(Number(where));
    if (offsets.length == 0) {
        print("Unable to break at line " + where);
        return;
    }
    for (var offset of offsets) {
        script.setBreakpoint(offset, { hit: handleBreakpoint });
    }
    print("Set breakpoint in " + script.url + ":" + script.startLine + " at line " + where + ", " + offsets.length);
}

// Build the table of commands.
var commands = {};
var commandArray = [
    backtraceCommand, "bt", "where",
    breakpointCommand, "b", "break",
    continueCommand, "c",
    detachCommand,
    downCommand, "d",
    evalCommand, "!",
    forcereturnCommand,
    frameCommand, "f",
    finishCommand, "fin",
    nextCommand, "n",
    printCommand, "p",
    keysCommand, "k",
    quitCommand, "q",
    runCommand, "run",
    stepCommand, "s",
    setCommand,
    throwCommand, "t",
    upCommand, "u",
    helpCommand, "h",
];
var currentCmd = null;
for (var i = 0; i < commandArray.length; i++) {
    var cmd = commandArray[i];
    if (typeof cmd === "string")
        commands[cmd] = currentCmd;
    else
        currentCmd = commands[cmd.name.replace(/Command$/, '')] = cmd;
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
// with punctuation, treat that as a separate word. The first word is
// terminated with whitespace or the '/' character. So:
//
//   print x         => ['print', 'x']
//   print           => ['print', '']
//   !print x        => ['!', 'print x']
//   ?!wtf!?         => ['?', '!wtf!?']
//   print/b x       => ['print', '/b x']
//
function breakcmd(cmd) {
    cmd = cmd.trimLeft();
    if ("!@#$%^&*_+=/?.,<>:;'\"".indexOf(cmd.substr(0, 1)) != -1)
        return [cmd.substr(0, 1), cmd.substr(1).trimLeft()];
    var m = /\s+|(?=\/)/.exec(cmd);
    if (m === null)
        return [cmd, ''];
    return [cmd.slice(0, m.index), cmd.slice(m.index + m[0].length)];
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

function preReplCleanups() {
    while (replCleanups.length > 0)
        replCleanups.pop()();
}

var prevcmd = undefined;
function repl() {
    preReplCleanups();

    var cmd;
    for (;;) {
        putstr("\n" + prompt);
        cmd = readline();
        if (cmd === null)
            return null;
        else if (cmd === "")
            cmd = prevcmd;

        try {
            prevcmd = cmd;
            var result = runcmd(cmd);
            if (result === undefined)
                ; // do nothing, return to prompt
            else if (Array.isArray(result))
                return result[0];
            else if (result === null)
                return null;
            else
                throw new Error("Internal error: result of runcmd wasn't array or undefined: " + result);
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
            updateLocation(focusedFrame);
            backtrace();
            return describedRv(repl(), "debugger.saveExc");
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

function handleBreakpoint (frame) {
    print("Breakpoint hit!");
    return saveExcursion(() => {
        topFrame = focusedFrame = frame;
        print("breakpoint hit.");
        showFrame();
        updateLocation(focusedFrame);
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

var args = scriptArgs.slice(0);
print("INITIAL ARGS: " + args);

// Find the script to run and its arguments. The script may have been given as
// a plain script name, in which case all remaining arguments belong to the
// script. Or there may have been any number of arguments to the JS shell,
// followed by -f scriptName, followed by additional arguments to the JS shell,
// followed by the script arguments. There may be multiple -e or -f options in
// the JS shell arguments, and we want to treat each one as a debuggable
// script.
//
// The difficulty is that the JS shell has a mixture of
//
//   --boolean
//
// and
//
//   --value VAL
//
// parameters, and there's no way to know whether --option takes an argument or
// not. We will assume that VAL will never end in .js, or rather that the first
// argument that does not start with "-" but does end in ".js" is the name of
// the script.
//
// If you need to pass other options and not have them given to the script,
// pass them before the -f jorendb.js argument. Thus, the safe ways to pass
// arguments are:
//
//   js [JS shell options] -f jorendb.js (-e SCRIPT | -f FILE)+ -- [script args]
//   js [JS shell options] -f jorendb.js (-e SCRIPT | -f FILE)* script.js [script args]
//
// Additionally, if you want to run a script that is *NOT* debugged, put it in
// as part of the leading [JS shell options].


// Compute actualScriptArgs by finding the script to be run and grabbing every
// non-script argument. The script may be given by -f scriptname or just plain
// scriptname. In the latter case, it will be in the global variable
// 'scriptPath' (and NOT in scriptArgs.)
var actualScriptArgs = [];
var scriptSeen;

if (scriptPath !== undefined) {
    todo.push({
        'action': 'load',
        'script': scriptPath,
    });
    scriptSeen = true;
}

while(args.length > 0) {
    var arg = args.shift();
    print("arg: " + arg);
    if (arg == '-e') {
        print("  eval");
        todo.push({
            'action': 'eval',
            'code': args.shift()
        });
    } else if (arg == '-f') {
        var script = args.shift();
        print("  load -f " + script);
        scriptSeen = true;
        todo.push({
            'action': 'load',
            'script': script,
        });
    } else if (arg.indexOf("-") == 0) {
        if (arg == '--') {
            print("  pass remaining args to script");
            actualScriptArgs.push(...args);
            break;
        } else if ((args.length > 0) && (args[0].indexOf(".js") + 3 == args[0].length)) {
            // Ends with .js, assume we are looking at --boolean script.js
            print("  load script.js after --boolean");
            todo.push({
                'action': 'load',
                'script': args.shift(),
            });
            scriptSeen = true;
        } else {
            // Does not end with .js, assume we are looking at JS shell arg
            // --value VAL
            print("  ignore");
            args.shift();
        }
    } else {
        if (!scriptSeen) {
            print("  load general");
            actualScriptArgs.push(...args);
            todo.push({
                'action': 'load',
                'script': arg,
            });
            break;
        } else {
            print("  arg " + arg);
            actualScriptArgs.push(arg);
        }
    }
}
print("jorendb: scriptPath = " + scriptPath);
print("jorendb: scriptArgs = " + scriptArgs);
print("jorendb: actualScriptArgs = " + actualScriptArgs);

for (var task of todo) {
    task['scriptArgs'] = actualScriptArgs;
}

// If nothing to run, just drop into a repl
if (todo.length == 0) {
    todo.push({ 'action': 'repl' });
}

while (rerun) {
    print("Top of run loop");
    rerun = false;
    for (var task of todo) {
        activeTask = task;
        if (task.action == 'eval') {
            debuggeeGlobal.eval(task.code);
        } else if (task.action == 'load') {
            debuggeeGlobal['scriptArgs'] = task.scriptArgs;
            debuggeeGlobal['scriptPath'] = task.script;
            print("Loading JavaScript file " + task.script);
            debuggeeGlobal.evaluate(read(task.script), { 'fileName': task.script, 'lineNumber': 1 });
        } else if (task.action == 'repl') {
            repl();
        }
        if (rerun)
            break;
    }
}

quit(0);
