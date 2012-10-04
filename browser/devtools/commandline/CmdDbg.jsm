/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

let EXPORTED_SYMBOLS = [ ];

Cu.import("resource:///modules/devtools/gcli.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

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
    let win = context.environment.chromeDocument.defaultView;
    let tab = win.gBrowser.selectedTab;
    let dbg = win.DebuggerUI.findDebugger();

    if (dbg) {
      if (dbg.ownerTab !== tab) {
        win.DebuggerUI.toggleDebugger();
      }

      return;
    }

    win.DebuggerUI.toggleDebugger();
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
    let win = context.environment.chromeDocument.defaultView;
    let tab = win.gBrowser.selectedTab;
    let dbg = win.DebuggerUI.findDebugger();

    if (dbg) {
      dbg.close();
    }
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
    let win = context.environment.chromeDocument.defaultView;
    let dbg = win.DebuggerUI.getDebugger();

    if (dbg) {
      let controller = dbg.contentWindow.DebuggerController;
      let thread = controller.activeThread;
      if (!thread.paused) {
        thread.interrupt();
      }
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
    let win = context.environment.chromeDocument.defaultView;
    let dbg = win.DebuggerUI.getDebugger();

    if (dbg) {
      let controller = dbg.contentWindow.DebuggerController;
      let thread = controller.activeThread;
      if (thread.paused) {
        thread.resume();
      }
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
    let win = context.environment.chromeDocument.defaultView;
    let dbg = win.DebuggerUI.getDebugger();

    if (dbg) {
      let controller = dbg.contentWindow.DebuggerController;
      let thread = controller.activeThread;
      if (thread.paused) {
        thread.stepOver();
      }
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
    let win = context.environment.chromeDocument.defaultView;
    let dbg = win.DebuggerUI.getDebugger();

    if (dbg) {
      let controller = dbg.contentWindow.DebuggerController;
      let thread = controller.activeThread;
      if (thread.paused) {
        thread.stepIn();
      }
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
    let win = context.environment.chromeDocument.defaultView;
    let dbg = win.DebuggerUI.getDebugger();

    if (dbg) {
      let controller = dbg.contentWindow.DebuggerController;
      let thread = controller.activeThread;
      if (thread.paused) {
        thread.stepOut();
      }
    }
  }
});
