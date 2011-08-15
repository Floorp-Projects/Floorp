/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * jorendb - A toy command-line debugger for shell-js programs.
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is jorendb toy JS debugger.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jason Orendorff <jorendorff@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * jorendb is a simple command-line debugger for shell-js programs.  It is
 * intended as a demo of the Debug object (as there are no shell js programs to
 * speak of).
 *
 * To run it: $JS -d path/to/this/file/jorendb.js
 * To run some JS code under it, try:
 *    (jorendb) print load("my-script-to-debug.js")
 * Execution will stop at debugger statements and you'll get a jorendb prompt.
 */

(function () {
    var debuggerSource = "(" + function () {
        // Debugger state.
        var focusedFrame = null;
        var topFrame = null;
        var debuggeeValues = [null];
        var lastExc = null;

        // Convert a debuggee value v to a string.
        function dvToString(v) {
            return (typeof v !== 'object' || v === null) ? uneval(v) : "[object " + v.class + "]";
        }

        function showDebuggeeValue(dv) {
            var dvrepr = dvToString(dv);
            var i = debuggeeValues.length;
            debuggeeValues[i] = dv;
            print("$" + i + " = " + dvrepr);
        }

        Object.defineProperty(Debug.Frame.prototype, "num", {
            configurable: true,
            enumerable: false,
            get: function () {
                    var i = 0;
                    for (var f = topFrame; f && f !== this; f = f.older)
                        i++;
                    return f === null ? undefined : i;
                }
            });

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

            var me;
            if (f.type === "call") {
                me = f.callee.name + "(" + f.arguments.map(dvToString).join(", ") + ")";
            } else {
                me = f.type + " code";
            }
            print("#" + n + " in " + me);
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
            if (focusedFrame === null) {
                // This is super bogus, need a way to create an env wrapping the debuggeeGlobal
                // and eval against that.
                var nonwrappedValue;
                try {
                    nonwrappedValue = debuggeeGlobal.eval(rest);
                } catch (exc) {
                    print("Exception caught.");
                    nonwrappedValue = exc;
                }
                if (typeof nonwrappedValue !== "object" || nonwrappedValue === null) {
                    // primitive value, no sweat
                    print("    " + uneval(nonwrappedValue));
                } else {
                    // junk for now
                    print("    " + Object.prototype.toString.call(nonwrappedValue));
                }
            } else {
                // This is the real deal.
                var cv = saveExcursion(function () { return focusedFrame.eval(rest); });
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
                for (var i = 0; i < n && f; i++)
                    f = f.older;
                if (f === null) {
                    print("There is no frame " + rest + ".");
                    return;
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

        function debugmeCommand() {
            var meta = newGlobal("new-compartment");
            meta.debuggeeGlobal = this;
            meta.debuggerSource = debuggerSource;
            meta.prompt = prompt.replace('(', '(meta-');
            meta.eval(debuggerSource);
        }

        function upCommand() {
            if (focusedFrame === null)
                print("No stack.");
            else if (focusedFrame.older === null)
                print("Initial frame selected; you cannot go up.");
            else {
                focusedFrame = focusedFrame.older;
                showFrame();
            }
        }

        function downCommand() {
            print("ugh");
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

        // Build the table of commands.
        var commands = {};
        var commandArray = [
            backtraceCommand, "bt", "where",
            continueCommand, "c",
            detachCommand,
            debugmeCommand,
            downCommand, "d",
            forcereturnCommand,
            frameCommand, "f",
            printCommand, "p",
            quitCommand, "q",
            throwCommand, "t",
            upCommand, "u"
            ];
        var last = null;
        for (var i = 0; i < commandArray.length; i++) {
            var cmd = commandArray[i];
            if (typeof cmd === "string")
                commands[cmd] = last;
            else
                last = commands[cmd.name.replace(/Command$/, '')] = cmd;
        }

        // Break cmd into two parts: its first word and everything else.
        function breakcmd(cmd) {
            cmd = cmd.trimLeft();
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
            var cmd;
            for (;;) {
                print("\n" + prompt);
                cmd = readline();
                if (cmd === null)
                    break;

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
                    var me = prompt.replace(/^\((.*)\)$/, function (a, b) { return b; });
                    print("Debug " + me + "? (y/n)");
                    if (readline().match(/^\s*y/i) !== null)
                        debugMe();
                    else
                        print("ok, ignoring error");
                }
            }
        }

        var dbg = new Debug(debuggeeGlobal);
        dbg.hooks = {
            debuggerHandler: function (frame) {
                return saveExcursion(function () {
                        topFrame = focusedFrame = frame;
                        print("'debugger' statement hit.");
                        showFrame();
                        return repl();
                    });
            },
            throw: function (frame, exc) {
                return saveExcursion(function () {
                        topFrame = focusedFrame = frame;
                        print("Unwinding due to exception. (Type 'c' to continue unwinding.)");
                        showFrame();
                        print("Exception value is:");
                        showDebuggeeValue(exc);
                        return repl();
                    });
            }
        };
        repl();
    } + ")();"

    print("jorendb version -0.0");
    var g = newGlobal("new-compartment");
    g.debuggeeGlobal = this;
    g.prompt = '(jorendb)';
    g.debuggerSource = debuggerSource;
    g.eval(debuggerSource);
})();
