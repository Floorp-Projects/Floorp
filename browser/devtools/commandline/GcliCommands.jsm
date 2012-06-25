/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


let EXPORTED_SYMBOLS = [ "GcliCommands" ];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource:///modules/devtools/gcli.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "HUDService",
                                  "resource:///modules/HUDService.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LayoutHelpers",
                                  "resource:///modules/devtools/LayoutHelpers.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "console",
                                  "resource:///modules/devtools/Console.jsm");

let prefSvc = "@mozilla.org/preferences-service;1";
XPCOMUtils.defineLazyGetter(this, "prefBranch", function() {
  let prefService = Cc[prefSvc].getService(Ci.nsIPrefService);
  return prefService.getBranch(null).QueryInterface(Ci.nsIPrefBranch2);
});

Cu.import("resource:///modules/devtools/GcliTiltCommands.jsm", {});

/**
 * A place to store the names of the commands that we have added as a result of
 * calling refreshAutoCommands(). Used by refreshAutoCommands to remove the
 * added commands.
 */
let commands = [];

/**
 * Exported API
 */
let GcliCommands = {
  /**
   * Called to look in a directory pointed at by the devtools.commands.dir pref
   * for *.mozcmd files which are then loaded.
   * @param nsIPrincipal aSandboxPrincipal Scope object for the Sandbox in which
   * we eval the script from the .mozcmd file. This should be a chrome window.
   */
  refreshAutoCommands: function GC_refreshAutoCommands(aSandboxPrincipal) {
    // First get rid of the last set of commands
    commands.forEach(function(name) {
      gcli.removeCommand(name);
    });

    let dirName = prefBranch.getComplexValue("devtools.commands.dir",
                                             Ci.nsISupportsString).data;
    if (dirName == "") {
      return;
    }

    let dir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
    dir.initWithPath(dirName);
    if (!dir.exists() || !dir.isDirectory()) {
      throw new Error('\'' + dirName + '\' is not a directory.');
    }

    let en = dir.directoryEntries.QueryInterface(Ci.nsIDirectoryEnumerator);

    while (true) {
      let file = en.nextFile;
      if (!file) {
        break;
      }
      if (file.leafName.match(/.*\.mozcmd$/) && file.isFile() && file.isReadable()) {
        loadCommandFile(file, aSandboxPrincipal);
      }
    }
  },
};

/**
 * Load the commands from a single file
 * @param nsIFile aFile The file containing the commands that we should read
 * @param nsIPrincipal aSandboxPrincipal Scope object for the Sandbox in which
 * we eval the script from the .mozcmd file. This should be a chrome window.
 */
function loadCommandFile(aFile, aSandboxPrincipal) {
  NetUtil.asyncFetch(aFile, function refresh_fetch(aStream, aStatus) {
    if (!Components.isSuccessCode(aStatus)) {
      console.error("NetUtil.asyncFetch(" + aFile.path + ",..) failed. Status=" + aStatus);
      return;
    }

    let source = NetUtil.readInputStreamToString(aStream, aStream.available());
    aStream.close();

    let sandbox = new Cu.Sandbox(aSandboxPrincipal, {
      sandboxPrototype: aSandboxPrincipal,
      wantXrays: false,
      sandboxName: aFile.path
    });
    let data = Cu.evalInSandbox(source, sandbox, "1.8", aFile.leafName, 1);

    if (!Array.isArray(data)) {
      console.error("Command file '" + aFile.leafName + "' does not have top level array.");
      return;
    }

    data.forEach(function(commandSpec) {
      gcli.addCommand(commandSpec);
      commands.push(commandSpec.name);
    });
  }.bind(this));
}

/**
 * 'cmd' command
 */
gcli.addCommand({
  name: "cmd",
  description: gcli.lookup("cmdDesc"),
});

/**
 * 'cmd refresh' command
 */
gcli.addCommand({
  name: "cmd refresh",
  description: gcli.lookup("cmdRefreshDesc"),
  exec: function Command_cmdRefresh(args, context) {
    GcliCommands.refreshAutoCommands(context.environment.chromeDocument.defaultView);
  }
});

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
 * 'screenshot' command
 */
gcli.addCommand({
  name: "screenshot",
  description: gcli.lookup("screenshotDesc"),
  manual: gcli.lookup("screenshotManual"),
  returnType: "string",
  params: [
    {
      name: "filename",
      type: "string",
      description: gcli.lookup("screenshotFilenameDesc"),
      manual: gcli.lookup("screenshotFilenameManual")
    },
    {
      name: "delay",
      type: { name: "number", min: 0 },
      defaultValue: 0,
      description: gcli.lookup("screenshotDelayDesc"),
      manual: gcli.lookup("screenshotDelayManual")
    },
    {
      name: "fullpage",
      type: "boolean",
      defaultValue: false,
      description: gcli.lookup("screenshotFullPageDesc"),
      manual: gcli.lookup("screenshotFullPageManual")
    },
    {
      name: "node",
      type: "node",
      defaultValue: null,
      description: gcli.lookup("inspectNodeDesc"),
      manual: gcli.lookup("inspectNodeManual")
    }
  ],
  exec: function Command_screenshot(args, context) {
    var document = context.environment.contentDocument;
    if (args.delay > 0) {
      var promise = context.createPromise();
      document.defaultView.setTimeout(function Command_screenshotDelay() {
        let reply = this.grabScreen(document, args.filename);
        promise.resolve(reply);
      }.bind(this), args.delay * 1000);
      return promise;
    }
    else {
      return this.grabScreen(document, args.filename, args.fullpage, args.node);
    }
  },
  grabScreen:
  function Command_screenshotGrabScreen(document, filename, fullpage, node) {
    let window = document.defaultView;
    let canvas = document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
    let left = 0;
    let top = 0;
    let width;
    let height;

    if (!fullpage) {
      if (!node) {
        left = window.scrollX;
        top = window.scrollY;
        width = window.innerWidth;
        height = window.innerHeight;
      } else {
        let rect = LayoutHelpers.getRect(node, window);
        top = rect.top;
        left = rect.left;
        width = rect.width;
        height = rect.height;
      }
    } else {
      width = window.innerWidth + window.scrollMaxX;
      height = window.innerHeight + window.scrollMaxY;
    }
    canvas.width = width;
    canvas.height = height;

    let ctx = canvas.getContext("2d");
    ctx.drawWindow(window, left, top, width, height, "#fff");

    let data = canvas.toDataURL("image/png", "");
    let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);

    // Check there is a .png extension to filename
    if (!filename.match(/.png$/i)) {
      filename += ".png";
    }

    // If the filename is relative, tack it onto the download directory
    if (!filename.match(/[\\\/]/)) {
      let downloadMgr = Cc["@mozilla.org/download-manager;1"]
        .getService(Ci.nsIDownloadManager);
      let tempfile = downloadMgr.userDownloadsDirectory;
      tempfile.append(filename);
      filename = tempfile.path;
    }

    try {
      file.initWithPath(filename);
    } catch (ex) {
      return "Error saving to " + filename;
    }

    let ioService = Cc["@mozilla.org/network/io-service;1"]
      .getService(Ci.nsIIOService);

    let Persist = Ci.nsIWebBrowserPersist;
    let persist = Cc["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"]
      .createInstance(Persist);
    persist.persistFlags = Persist.PERSIST_FLAGS_REPLACE_EXISTING_FILES |
                           Persist.PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION;

    let source = ioService.newURI(data, "UTF8", null);
    persist.saveURI(source, null, null, null, null, file);

    return "Saved to " + filename;
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
    let threadManager = Cc["@mozilla.org/thread-manager;1"]
        .getService(Ci.nsIThreadManager);
    threadManager.mainThread.dispatch({
      run: function() {
        hud.gcliterm.clearOutput();
      }
    }, Ci.nsIThread.DISPATCH_NORMAL);
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
  manual: gcli.lookup("editManual2"),
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
     let win = HUDService.currentContext();
     win.StyleEditor.openChrome(args.resource.element, args.line);
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
