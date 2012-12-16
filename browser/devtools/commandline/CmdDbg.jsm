/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

this.EXPORTED_SYMBOLS = [ ];

Cu.import("resource:///modules/devtools/gcli.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "gDevTools",
                                  "resource:///modules/devtools/gDevTools.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TargetFactory",
                                  "resource:///modules/devtools/Target.jsm");

/**
 * 'dbg' command
 */
gcli.addCommand({
  name: "dbg",
  description: gcli.lookup("dbgDesc"),
  manual: gcli.lookup("dbgManual")
});

/**
 * 'dbg open' command
 */
gcli.addCommand({
  name: "dbg open",
  description: gcli.lookup("dbgOpen"),
  params: [],
  exec: function (args, context) {
    let gBrowser = context.environment.chromeDocument.defaultView.gBrowser;
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    return gDevTools.showToolbox(target, "jsdebugger");
  }
});

/**
 * 'dbg close' command
 */
gcli.addCommand({
  name: "dbg close",
  description: gcli.lookup("dbgClose"),
  params: [],
  exec: function (args, context) {
    let gBrowser = context.environment.chromeDocument.defaultView.gBrowser;
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    return gDevTools.closeToolbox(target);
  }
});

/**
 * 'dbg interrupt' command
 */
gcli.addCommand({
  name: "dbg interrupt",
  description: gcli.lookup("dbgInterrupt"),
  params: [],
  exec: function(args, context) {
    let dbg = getPanel(context, "jsdebugger");
    if (!dbg) {
      return gcli.lookup("debuggerStopped");
    }

    let controller = dbg._controller;
    let thread = controller.activeThread;
    if (!thread.paused) {
      thread.interrupt();
    }
  }
});

/**
 * 'dbg continue' command
 */
gcli.addCommand({
  name: "dbg continue",
  description: gcli.lookup("dbgContinue"),
  params: [],
  exec: function(args, context) {
    let dbg = getPanel(context, "jsdebugger");
    if (!dbg) {
      return gcli.lookup("debuggerStopped");
    }

    let controller = dbg._controller;
    let thread = controller.activeThread;
    if (thread.paused) {
      thread.resume();
    }
  }
});

/**
 * 'dbg step' command
 */
gcli.addCommand({
  name: "dbg step",
  description: gcli.lookup("dbgStepDesc"),
  manual: gcli.lookup("dbgStepManual")
});

/**
 * 'dbg step over' command
 */
gcli.addCommand({
  name: "dbg step over",
  description: gcli.lookup("dbgStepOverDesc"),
  params: [],
  exec: function(args, context) {
    let dbg = getPanel(context, "jsdebugger");
    if (!dbg) {
      return gcli.lookup("debuggerStopped");
    }

    let controller = dbg._controller;
    let thread = controller.activeThread;
    if (thread.paused) {
      thread.stepOver();
    }
  }
});

/**
 * 'dbg step in' command
 */
gcli.addCommand({
  name: 'dbg step in',
  description: gcli.lookup("dbgStepInDesc"),
  params: [],
  exec: function(args, context) {
    let dbg = getPanel(context, "jsdebugger");
    if (!dbg) {
      return gcli.lookup("debuggerStopped");
    }

    let controller = dbg._controller;
    let thread = controller.activeThread;
    if (thread.paused) {
      thread.stepIn();
    }
  }
});

/**
 * 'dbg step over' command
 */
gcli.addCommand({
  name: 'dbg step out',
  description: gcli.lookup("dbgStepOutDesc"),
  params: [],
  exec: function(args, context) {
    let dbg = getPanel(context, "jsdebugger");
    if (!dbg) {
      return gcli.lookup("debuggerStopped");
    }

    let controller = dbg._controller;
    let thread = controller.activeThread;
    if (thread.paused) {
      thread.stepOut();
    }
  }
});

/**
 * A helper to go from a command context to a debugger panel
 */
function getPanel(context, id) {
  let gBrowser = context.environment.chromeDocument.defaultView.gBrowser;
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = gDevTools.getToolbox(target);
  return toolbox == null ? undefined : toolbox.getPanel(id);
}
