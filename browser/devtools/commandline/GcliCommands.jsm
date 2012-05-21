/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


let EXPORTED_SYMBOLS = [ ];

Components.utils.import("resource:///modules/devtools/gcli.jsm");
Components.utils.import("resource:///modules/HUDService.jsm");

Components.utils.import("resource:///modules/devtools/GcliTiltCommands.jsm", {});


/**
 * 'echo' command
 */
gcli.addCommand({
  name: "echo",
  description: gcli.lookup("echoDesc"),
  params: [
    {
      name: "message",
      type: "string",
      description: gcli.lookup("echoMessageDesc")
    }
  ],
  returnType: "string",
  exec: function Command_echo(args, context) {
    return args.message;
  }
});


/**
 * 'console' command
 */
gcli.addCommand({
  name: "console",
  description: gcli.lookup("consoleDesc"),
  manual: gcli.lookup("consoleManual")
});

/**
 * 'console clear' command
 */
gcli.addCommand({
  name: "console clear",
  description: gcli.lookup("consoleclearDesc"),
  exec: function Command_consoleClear(args, context) {
    let window = context.environment.chromeDocument.defaultView;
    let hud = HUDService.getHudReferenceById(context.environment.hudId);

    // Use a timeout so we also clear the reporting of the clear command
    let threadManager = Components.classes["@mozilla.org/thread-manager;1"]
        .getService(Components.interfaces.nsIThreadManager);
    threadManager.mainThread.dispatch({
      run: function() {
        hud.gcliterm.clearOutput();
      }
    }, Components.interfaces.nsIThread.DISPATCH_NORMAL);
  }
});


/**
 * 'console close' command
 */
gcli.addCommand({
  name: "console close",
  description: gcli.lookup("consolecloseDesc"),
  exec: function Command_consoleClose(args, context) {
    let tab = context.environment.chromeDocument.defaultView.gBrowser.selectedTab
    HUDService.deactivateHUDForContext(tab);
  }
});

/**
 * 'console open' command
 */
gcli.addCommand({
  name: "console open",
  description: gcli.lookup("consoleopenDesc"),
  exec: function Command_consoleOpen(args, context) {
    let tab = context.environment.chromeDocument.defaultView.gBrowser.selectedTab
    HUDService.activateHUDForContext(tab);
  }
});

/**
 * 'inspect' command
 */
gcli.addCommand({
  name: "inspect",
  description: gcli.lookup("inspectDesc"),
  manual: gcli.lookup("inspectManual"),
  params: [
    {
      name: "node",
      type: "node",
      description: gcli.lookup("inspectNodeDesc"),
      manual: gcli.lookup("inspectNodeManual")
    }
  ],
  exec: function Command_inspect(args, context) {
    let document = context.environment.chromeDocument;
    document.defaultView.InspectorUI.openInspectorUI(args.node);
  }
});

/**
 * 'edit' command
 */
gcli.addCommand({
  name: "edit",
  description: gcli.lookup("editDesc"),
  manual: gcli.lookup("editManual"),
  params: [
     {
       name: 'resource',
       type: {
         name: 'resource',
         include: 'text/css'
       },
       description: gcli.lookup("editResourceDesc")
     },
     {
       name: "line",
       defaultValue: 1,
       type: {
         name: "number",
         min: 1,
         step: 10
       },
       description: gcli.lookup("editLineToJumpToDesc")
     }
   ],
   exec: function(args, context) {
     let hud = HUDService.getHudReferenceById(context.environment.hudId);
     let StyleEditor = hud.gcliterm.document.defaultView.StyleEditor;
     StyleEditor.openChrome(args.resource.element, args.line);
   }
});

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
    let dbg = win.DebuggerUI.getDebugger(win.gBrowser.selectedTab);
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
          let dbg = win.DebuggerUI.getDebugger(win.gBrowser.selectedTab);
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
    let dbg = win.DebuggerUI.getDebugger(win.gBrowser.selectedTab);
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
          let dbg = win.DebuggerUI.getDebugger(win.gBrowser.selectedTab);
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
    let dbg = win.DebuggerUI.getDebugger(win.gBrowser.selectedTab);
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
