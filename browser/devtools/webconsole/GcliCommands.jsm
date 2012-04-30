/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is GCLI Commands.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Walker <jwalker@mozilla.com> (original author)
 *   Mihai Sucan <mihai.sucan@gmail.com>
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


let EXPORTED_SYMBOLS = [ "GcliCommands" ];

Components.utils.import("resource:///modules/gcli.jsm");
Components.utils.import("resource:///modules/HUDService.jsm");


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
            let scriptsView = dbg.debuggerWindow.DebuggerView.Scripts;
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
