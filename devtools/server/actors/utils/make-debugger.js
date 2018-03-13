/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const Debugger = require("Debugger");

const { reportException } = require("devtools/shared/DevToolsUtils");

/**
 * Multiple actors that use a |Debugger| instance come in a few versions, each
 * with a different set of debuggees. One version for content tabs (globals
 * within a tab), one version for chrome debugging (all globals), and sometimes
 * a third version for addon debugging (chrome globals the addon is loaded in
 * and content globals the addon injects scripts into). The |makeDebugger|
 * function helps us avoid repeating the logic for finding and maintaining the
 * correct set of globals for a given |Debugger| instance across each version of
 * all of our actors.
 *
 * The |makeDebugger| function expects a single object parameter with the
 * following properties:
 *
 * @param Function findDebuggees
 *        Called with one argument: a |Debugger| instance. This function should
 *        return an iterable of globals to be added to the |Debugger|
 *        instance. The globals may be wrapped in a |Debugger.Object|, or
 *        unwrapped.
 *
 * @param Function shouldAddNewGlobalAsDebuggee
 *        Called with one argument: a |Debugger.Object| wrapping a global
 *        object. This function must return |true| if the global object should
 *        be added as debuggee, and |false| otherwise.
 *
 * @returns Debugger
 *          Returns a |Debugger| instance that can manage its set of debuggee
 *          globals itself and is decorated with the |EventEmitter| class.
 *
 *          Events emitted by the returned |Debugger| instance:
 *
 *            - "newGlobal": Emitted when a new global has been added as a
 *               debuggee. Passes the |Debugger.Object| wrapping the new
 *               debuggee global to listeners.
 *
 *          Existing |Debugger| properties set on the returned |Debugger|
 *          instance:
 *
 *            - onNewGlobalObject: The |Debugger| will automatically add new
 *              globals as debuggees if calling |shouldAddNewGlobalAsDebuggee|
 *              with the global returns true.
 *
 *            - uncaughtExceptionHook: The |Debugger| already has an error
 *              reporter attached to |uncaughtExceptionHook|, so if any
 *              |Debugger| hooks fail, the error will be reported.
 *
 *          New properties set on the returned |Debugger| instance:
 *
 *            - addDebuggees: A function which takes no arguments. It adds all
 *              current globals that should be debuggees (as determined by
 *              |findDebuggees|) to the |Debugger| instance.
 */
module.exports = function makeDebugger({ findDebuggees, shouldAddNewGlobalAsDebuggee }) {
  const dbg = new Debugger();
  EventEmitter.decorate(dbg);

  dbg.allowUnobservedAsmJS = true;
  dbg.uncaughtExceptionHook = reportDebuggerHookException;

  dbg.onNewGlobalObject = function(global) {
    if (shouldAddNewGlobalAsDebuggee(global)) {
      safeAddDebuggee(this, global);
    }
  };

  dbg.addDebuggees = function() {
    for (let global of findDebuggees(this)) {
      safeAddDebuggee(this, global);
    }
  };

  return dbg;
};

const reportDebuggerHookException = e => reportException("Debugger Hook", e);

/**
 * Add |global| as a debuggee to |dbg|, handling error cases.
 */
function safeAddDebuggee(dbg, global) {
  try {
    let wrappedGlobal = dbg.addDebuggee(global);
    if (wrappedGlobal) {
      dbg.emit("newGlobal", wrappedGlobal);
    }
  } catch (e) {
    // Ignoring attempt to add the debugger's compartment as a debuggee.
  }
}
