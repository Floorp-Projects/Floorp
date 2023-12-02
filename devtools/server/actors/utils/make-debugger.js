/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("resource://devtools/shared/event-emitter.js");
const Debugger = require("Debugger");

const {
  reportException,
} = require("resource://devtools/shared/DevToolsUtils.js");

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
 *        instance. The globals are the actual global objects and aren't wrapped
 *        in in a |Debugger.Object|.
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
module.exports = function makeDebugger({
  findDebuggees,
  shouldAddNewGlobalAsDebuggee,
} = {}) {
  const dbg = new Debugger();
  EventEmitter.decorate(dbg);

  // By default, we disable asm.js and WASM debugging because of performance reason.
  // Enabling asm.js debugging (allowUnobservedAsmJS=false) will make asm.js fallback to JS compiler
  // and be debugging as a regular JS script.
  dbg.allowUnobservedAsmJS = true;
  // Enabling WASM debugging (allowUnobservedWasm=false) will make the engine compile WASM scripts
  // into different machine code with debugging instructions. This significantly increase the memory usage of it.
  dbg.allowUnobservedWasm = true;

  dbg.uncaughtExceptionHook = reportDebuggerHookException;

  const onNewGlobalObject = function (global) {
    if (shouldAddNewGlobalAsDebuggee(global)) {
      safeAddDebuggee(this, global);
    }
  };

  dbg.onNewGlobalObject = onNewGlobalObject;
  dbg.addDebuggees = function () {
    for (const global of findDebuggees(this)) {
      safeAddDebuggee(this, global);
    }
  };

  dbg.disable = function () {
    dbg.removeAllDebuggees();
    dbg.onNewGlobalObject = undefined;
  };

  dbg.enable = function () {
    dbg.addDebuggees();
    dbg.onNewGlobalObject = onNewGlobalObject;
  };
  dbg.findDebuggees = function () {
    return findDebuggees(dbg);
  };

  return dbg;
};

const reportDebuggerHookException = e => reportException("DBG-SERVER", e);

/**
 * Add |global| as a debuggee to |dbg|, handling error cases.
 */
function safeAddDebuggee(dbg, global) {
  let globalDO;
  try {
    globalDO = dbg.addDebuggee(global);
  } catch (e) {
    // Ignoring attempt to add the debugger's compartment as a debuggee.
    return;
  }

  if (dbg.onNewDebuggee) {
    dbg.onNewDebuggee(globalDO);
  }
}
