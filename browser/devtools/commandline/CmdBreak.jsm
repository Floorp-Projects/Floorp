/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

this.EXPORTED_SYMBOLS = [ ];

Cu.import("resource:///modules/devtools/gcli.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "HUDService",
                                  "resource:///modules/HUDService.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "TargetFactory",
                                  "resource:///modules/devtools/Target.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "gDevTools",
                                  "resource:///modules/devtools/gDevTools.jsm");

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
    let dbg = getPanel(context, "jsdebugger");
    if (!dbg) {
      return gcli.lookup("debuggerStopped");
    }

    let breakpoints = dbg.getAllBreakpoints();

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
        data: function(args, context) {
          let files = [];
          let dbg = getPanel(context, "jsdebugger");
          if (dbg) {
            let sourcesView = dbg.panelWin.DebuggerView.Sources;
            for (let item in sourcesView) {
              files.push(item.value);
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

    let dbg = getPanel(context, "jsdebugger");
    if (!dbg) {
      return gcli.lookup("debuggerStopped");
    }
    var deferred = context.defer();
    let position = { url: args.file, line: args.line };
    dbg.addBreakpoint(position, function(aBreakpoint, aError) {
      if (aError) {
        deferred.resolve(gcli.lookupFormat("breakaddFailed", [aError]));
        return;
      }
      deferred.resolve(gcli.lookup("breakaddAdded"));
    });
    return deferred.promise;
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
        max: function(args, context) {
          let dbg = getPanel(context, "jsdebugger");
          return dbg == null ?
              null :
              Object.keys(dbg.getAllBreakpoints()).length - 1;
        },
      },
      description: gcli.lookup("breakdelBreakidDesc")
    }
  ],
  returnType: "html",
  exec: function(args, context) {
    let dbg = getPanel(context, "jsdebugger");
    if (!dbg) {
      return gcli.lookup("debuggerStopped");
    }

    let breakpoints = dbg.getAllBreakpoints();
    let id = Object.keys(breakpoints)[args.breakid];
    if (!id || !(id in breakpoints)) {
      return gcli.lookup("breakNotFound");
    }

    let deferred = context.defer();
    try {
      dbg.removeBreakpoint(breakpoints[id], function() {
        deferred.resolve(gcli.lookup("breakdelRemoved"));
      });
    } catch (ex) {
      // If the debugger has been closed already, don't scare the user.
      deferred.resolve(gcli.lookup("breakdelRemoved"));
    }
    return deferred.promise;
  }
});

/**
 * A helper to go from a command context to a debugger panel
 */
function getPanel(context, id) {
  if (context == null) {
    return undefined;
  }

  let gBrowser = context.environment.chromeDocument.defaultView.gBrowser;
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let toolbox = gDevTools.getToolbox(target);
  return toolbox == null ? undefined : toolbox.getPanel(id);
}
