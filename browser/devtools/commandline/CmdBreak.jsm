/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

let EXPORTED_SYMBOLS = [ ];

Cu.import("resource:///modules/devtools/gcli.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "HUDService",
                                  "resource:///modules/HUDService.jsm");

/**
 * 'break' command
 */
gcli.addCommand({
  name: "break",
  description: gcli.lookup("breakDesc"),
  manual: gcli.lookup("breakManual")
});


/**
 * 'break list' command
 */
gcli.addCommand({
  name: "break list",
  description: gcli.lookup("breaklistDesc"),
  returnType: "html",
  exec: function(args, context) {
    let win = HUDService.currentContext();
    let dbg = win.DebuggerUI.getDebugger();
    if (!dbg) {
      return gcli.lookup("breakaddDebuggerStopped");
    }
    let breakpoints = dbg.breakpoints;

    if (Object.keys(breakpoints).length === 0) {
      return gcli.lookup("breaklistNone");
    }

    let reply = gcli.lookup("breaklistIntro");
    reply += "<ol>";
    for each (let breakpoint in breakpoints) {
      let text = gcli.lookupFormat("breaklistLineEntry",
                                   [breakpoint.location.url,
                                    breakpoint.location.line]);
      reply += "<li>" + text + "</li>";
    };
    reply += "</ol>";
    return reply;
  }
});


/**
 * 'break add' command
 */
gcli.addCommand({
  name: "break add",
  description: gcli.lookup("breakaddDesc"),
  manual: gcli.lookup("breakaddManual")
});

/**
 * 'break add line' command
 */
gcli.addCommand({
  name: "break add line",
  description: gcli.lookup("breakaddlineDesc"),
  params: [
    {
      name: "file",
      type: {
        name: "selection",
        data: function() {
          let win = HUDService.currentContext();
          let dbg = win.DebuggerUI.getDebugger();
          let files = [];
          if (dbg) {
            let scriptsView = dbg.contentWindow.DebuggerView.Scripts;
            for each (let script in scriptsView.scriptLocations) {
              files.push(script);
            }
          }
          return files;
        }
      },
      description: gcli.lookup("breakaddlineFileDesc")
    },
    {
      name: "line",
      type: { name: "number", min: 1, step: 10 },
      description: gcli.lookup("breakaddlineLineDesc")
    }
  ],
  returnType: "html",
  exec: function(args, context) {
    args.type = "line";
    let win = HUDService.currentContext();
    let dbg = win.DebuggerUI.getDebugger();
    if (!dbg) {
      return gcli.lookup("breakaddDebuggerStopped");
    }
    var promise = context.createPromise();
    let position = { url: args.file, line: args.line };
    dbg.addBreakpoint(position, function(aBreakpoint, aError) {
      if (aError) {
        promise.resolve(gcli.lookupFormat("breakaddFailed", [aError]));
        return;
      }
      promise.resolve(gcli.lookup("breakaddAdded"));
    });
    return promise;
  }
});


/**
 * 'break del' command
 */
gcli.addCommand({
  name: "break del",
  description: gcli.lookup("breakdelDesc"),
  params: [
    {
      name: "breakid",
      type: {
        name: "number",
        min: 0,
        max: function() {
          let win = HUDService.currentContext();
          let dbg = win.DebuggerUI.getDebugger();
          if (!dbg) {
            return gcli.lookup("breakaddDebuggerStopped");
          }
          return Object.keys(dbg.breakpoints).length - 1;
        },
      },
      description: gcli.lookup("breakdelBreakidDesc")
    }
  ],
  returnType: "html",
  exec: function(args, context) {
    let win = HUDService.currentContext();
    let dbg = win.DebuggerUI.getDebugger();
    if (!dbg) {
      return gcli.lookup("breakaddDebuggerStopped");
    }

    let breakpoints = dbg.breakpoints;
    let id = Object.keys(dbg.breakpoints)[args.breakid];
    if (!id || !(id in breakpoints)) {
      return gcli.lookup("breakNotFound");
    }

    let promise = context.createPromise();
    try {
      dbg.removeBreakpoint(breakpoints[id], function() {
        promise.resolve(gcli.lookup("breakdelRemoved"));
      });
    } catch (ex) {
      // If the debugger has been closed already, don't scare the user.
      promise.resolve(gcli.lookup("breakdelRemoved"));
    }
    return promise;
  }
});
