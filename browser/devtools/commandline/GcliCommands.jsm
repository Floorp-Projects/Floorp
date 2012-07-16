/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


let EXPORTED_SYMBOLS = [ "GcliCommands" ];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

const XMLHttpRequest =
  Components.Constructor("@mozilla.org/xmlextras/xmlhttprequest;1");

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

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "js_beautify",
                                  "resource:///modules/devtools/Jsbeautify.jsm");

XPCOMUtils.defineLazyGetter(this, "Debugger", function() {
  let JsDebugger = {};
  Components.utils.import("resource://gre/modules/jsdebugger.jsm", JsDebugger);

  let global = Components.utils.getGlobalForObject({});
  JsDebugger.addDebuggerToGlobal(global);

  return global.Debugger;
});

let prefSvc = "@mozilla.org/preferences-service;1";
XPCOMUtils.defineLazyGetter(this, "prefBranch", function() {
  let prefService = Cc[prefSvc].getService(Ci.nsIPrefService);
  return prefService.getBranch(null).QueryInterface(Ci.nsIPrefBranch2);
});

Cu.import("resource:///modules/devtools/GcliTiltCommands.jsm", {});
Cu.import("resource:///modules/devtools/GcliCookieCommands.jsm", {});

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
  hidden: true
});

/**
 * 'cmd refresh' command
 */
gcli.addCommand({
  name: "cmd refresh",
  description: gcli.lookup("cmdRefreshDesc"),
  hidden: true,
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
  hidden: true,
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


let callLogDebuggers = [];

/**
 * 'calllog' command
 */
gcli.addCommand({
  name: "calllog",
  description: gcli.lookup("calllogDesc")
})

/**
 * 'calllog start' command
 */
gcli.addCommand({
  name: "calllog start",
  description: gcli.lookup("calllogStartDesc"),

  exec: function(args, context) {
    let contentWindow = context.environment.contentDocument.defaultView;

    let dbg = new Debugger(contentWindow);
    dbg.onEnterFrame = function(frame) {
      // BUG 773652 -  Make the output from the GCLI calllog command nicer
      contentWindow.console.log("Method call: " + this.callDescription(frame));
    }.bind(this);

    callLogDebuggers.push(dbg);

    let tab = context.environment.chromeDocument.defaultView.gBrowser.selectedTab;
    HUDService.activateHUDForContext(tab);

    return gcli.lookup("calllogStartReply");
  },

  callDescription: function(frame) {
    let name = "<anonymous>";
    if (frame.callee.name) {
      name = frame.callee.name;
    }
    else {
      let desc = frame.callee.getOwnPropertyDescriptor("displayName");
      if (desc && desc.value && typeof desc.value == "string") {
        name = desc.value;
      }
    }

    let args = frame.arguments.map(this.valueToString).join(", ");
    return name + "(" + args + ")";
  },

  valueToString: function(value) {
    if (typeof value !== "object" || value === null) {
      return uneval(value);
    }
    return "[object " + value.class + "]";
  }
});

/**
 * 'calllog stop' command
 */
gcli.addCommand({
  name: "calllog stop",
  description: gcli.lookup("calllogStopDesc"),

  exec: function(args, context) {
    let numDebuggers = callLogDebuggers.length;
    if (numDebuggers == 0) {
      return gcli.lookup("calllogStopNoLogging");
    }

    for (let dbg of callLogDebuggers) {
      dbg.onEnterFrame = undefined;
    }
    callLogDebuggers = [];

    return gcli.lookupFormat("calllogStopReply", [ numDebuggers ]);
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
    let window = context.environment.contentDocument.defaultView;
    let hud = HUDService.getHudByWindow(window);
    // hud will be null if the web console has not been opened for this window
    if (hud) {
      hud.jsterm.clearOutput();
    }
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
 * Restart command
 *
 * @param boolean nocache
 *        Disables loading content from cache upon restart.
 *
 * Examples :
 * >> restart
 * - restarts browser immediately
 * >> restart --nocache
 * - restarts immediately and starts Firefox without using cache
 */
gcli.addCommand({
  name: "restart",
  description: gcli.lookup("restartFirefoxDesc"),
  params: [
    {
      name: "nocache",
      type: "boolean",
      defaultValue: false,
      description: gcli.lookup("restartFirefoxNocacheDesc")
    }
  ],
  returnType: "string",
  exec: function Restart(args, context) {
    let canceled = Cc["@mozilla.org/supports-PRBool;1"]
                     .createInstance(Ci.nsISupportsPRBool);
    Services.obs.notifyObservers(canceled, "quit-application-requested", "restart");
    if (canceled.data) {
      return gcli.lookup("restartFirefoxRequestCancelled");
    }

    // disable loading content from cache.
    if (args.nocache) {
      Services.appinfo.invalidateCachesOnRestart();
    }

    // restart
    Cc['@mozilla.org/toolkit/app-startup;1']
      .getService(Ci.nsIAppStartup)
      .quit(Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart);
    return gcli.lookup("restartFirefoxRestarting");
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

/**
 * 'export' command
 */
gcli.addCommand({
  name: "export",
  description: gcli.lookup("exportDesc"),
});

/**
 * The 'export html' command. This command allows the user to export the page to
 * HTML after they do DOM changes.
 */
gcli.addCommand({
  name: "export html",
  description: gcli.lookup("exportHtmlDesc"),
  exec: function(args, context) {
    let document = context.environment.contentDocument;
    let window = document.defaultView;
    let page = document.documentElement.outerHTML;
    window.open('data:text/plain;charset=utf8,' + encodeURIComponent(page));
  }
});

/**
 * 'pagemod' command
 */
gcli.addCommand({
  name: "pagemod",
  description: gcli.lookup("pagemodDesc"),
});

/**
 * The 'pagemod replace' command. This command allows the user to search and
 * replace within text nodes and attributes.
 */
gcli.addCommand({
  name: "pagemod replace",
  description: gcli.lookup("pagemodReplaceDesc"),
  params: [
    {
      name: "search",
      type: "string",
      description: gcli.lookup("pagemodReplaceSearchDesc"),
    },
    {
      name: "replace",
      type: "string",
      description: gcli.lookup("pagemodReplaceReplaceDesc"),
    },
    {
      name: "ignoreCase",
      type: "boolean",
      description: gcli.lookup("pagemodReplaceIgnoreCaseDesc"),
    },
    {
      name: "selector",
      type: "string",
      description: gcli.lookup("pagemodReplaceSelectorDesc"),
      defaultValue: "*:not(script):not(style):not(embed):not(object):not(frame):not(iframe):not(frameset)",
    },
    {
      name: "root",
      type: "node",
      description: gcli.lookup("pagemodReplaceRootDesc"),
      defaultValue: null,
    },
    {
      name: "attrOnly",
      type: "boolean",
      description: gcli.lookup("pagemodReplaceAttrOnlyDesc"),
    },
    {
      name: "contentOnly",
      type: "boolean",
      description: gcli.lookup("pagemodReplaceContentOnlyDesc"),
    },
    {
      name: "attributes",
      type: "string",
      description: gcli.lookup("pagemodReplaceAttributesDesc"),
      defaultValue: null,
    },
  ],
  exec: function(args, context) {
    let document = context.environment.contentDocument;
    let searchTextNodes = !args.attrOnly;
    let searchAttributes = !args.contentOnly;
    let regexOptions = args.ignoreCase ? 'ig' : 'g';
    let search = new RegExp(escapeRegex(args.search), regexOptions);
    let attributeRegex = null;
    if (args.attributes) {
      attributeRegex = new RegExp(args.attributes, regexOptions);
    }

    let root = args.root || document;
    let elements = root.querySelectorAll(args.selector);
    elements = Array.prototype.slice.call(elements);

    let replacedTextNodes = 0;
    let replacedAttributes = 0;

    function replaceAttribute() {
      replacedAttributes++;
      return args.replace;
    }
    function replaceTextNode() {
      replacedTextNodes++;
      return args.replace;
    }

    for (let i = 0; i < elements.length; i++) {
      let element = elements[i];
      if (searchTextNodes) {
        for (let y = 0; y < element.childNodes.length; y++) {
          let node = element.childNodes[y];
          if (node.nodeType == node.TEXT_NODE) {
            node.textContent = node.textContent.replace(search, replaceTextNode);
          }
        }
      }

      if (searchAttributes) {
        if (!element.attributes) {
          continue;
        }
        for (let y = 0; y < element.attributes.length; y++) {
          let attr = element.attributes[y];
          if (!attributeRegex || attributeRegex.test(attr.name)) {
            attr.value = attr.value.replace(search, replaceAttribute);
          }
        }
      }
    }

    return gcli.lookupFormat("pagemodReplaceResult",
                             [elements.length, replacedTextNodes,
                              replacedAttributes]);
  }
});

/**
 * 'pagemod remove' command
 */
gcli.addCommand({
  name: "pagemod remove",
  description: gcli.lookup("pagemodRemoveDesc"),
});


/**
 * The 'pagemod remove element' command.
 */
gcli.addCommand({
  name: "pagemod remove element",
  description: gcli.lookup("pagemodRemoveElementDesc"),
  params: [
    {
      name: "search",
      type: "string",
      description: gcli.lookup("pagemodRemoveElementSearchDesc"),
    },
    {
      name: "root",
      type: "node",
      description: gcli.lookup("pagemodRemoveElementRootDesc"),
      defaultValue: null,
    },
    {
      name: 'stripOnly',
      type: 'boolean',
      description: gcli.lookup("pagemodRemoveElementStripOnlyDesc"),
    },
    {
      name: 'ifEmptyOnly',
      type: 'boolean',
      description: gcli.lookup("pagemodRemoveElementIfEmptyOnlyDesc"),
    },
  ],
  exec: function(args, context) {
    let document = context.environment.contentDocument;
    let root = args.root || document;
    let elements = Array.prototype.slice.call(root.querySelectorAll(args.search));

    let removed = 0;
    for (let i = 0; i < elements.length; i++) {
      let element = elements[i];
      let parentNode = element.parentNode;
      if (!parentNode || !element.removeChild) {
        continue;
      }
      if (args.stripOnly) {
        while (element.hasChildNodes()) {
          parentNode.insertBefore(element.childNodes[0], element);
        }
      }
      if (!args.ifEmptyOnly || !element.hasChildNodes()) {
        element.parentNode.removeChild(element);
        removed++;
      }
    }

    return gcli.lookupFormat("pagemodRemoveElementResultMatchedAndRemovedElements",
                             [elements.length, removed]);
  }
});

/**
 * The 'pagemod remove attribute' command.
 */
gcli.addCommand({
  name: "pagemod remove attribute",
  description: gcli.lookup("pagemodRemoveAttributeDesc"),
  params: [
    {
      name: "searchAttributes",
      type: "string",
      description: gcli.lookup("pagemodRemoveAttributeSearchAttributesDesc"),
    },
    {
      name: "searchElements",
      type: "string",
      description: gcli.lookup("pagemodRemoveAttributeSearchElementsDesc"),
    },
    {
      name: "root",
      type: "node",
      description: gcli.lookup("pagemodRemoveAttributeRootDesc"),
      defaultValue: null,
    },
    {
      name: "ignoreCase",
      type: "boolean",
      description: gcli.lookup("pagemodRemoveAttributeIgnoreCaseDesc"),
    },
  ],
  exec: function(args, context) {
    let document = context.environment.contentDocument;

    let root = args.root || document;
    let regexOptions = args.ignoreCase ? 'ig' : 'g';
    let attributeRegex = new RegExp(args.searchAttributes, regexOptions);
    let elements = root.querySelectorAll(args.searchElements);
    elements = Array.prototype.slice.call(elements);

    let removed = 0;
    for (let i = 0; i < elements.length; i++) {
      let element = elements[i];
      if (!element.attributes) {
        continue;
      }

      var attrs = Array.prototype.slice.call(element.attributes);
      for (let y = 0; y < attrs.length; y++) {
        let attr = attrs[y];
        if (attributeRegex.test(attr.name)) {
          element.removeAttribute(attr.name);
          removed++;
        }
      }
    }

    return gcli.lookupFormat("pagemodRemoveAttributeResult",
                             [elements.length, removed]);
  }
});


/**
 * Make a given string safe to use  in a regular expression.
 *
 * @param string aString
 *        The string you want to use in a regex.
 * @return string
 *         The equivalent of |aString| but safe to use in a regex.
 */
function escapeRegex(aString) {
  return aString.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, "\\$&");
}

/**
 * 'addon' command.
 */
gcli.addCommand({
  name: "addon",
  description: gcli.lookup("addonDesc")
});

/**
 * 'addon list' command.
 */
gcli.addCommand({
  name: "addon list",
  description: gcli.lookup("addonListDesc"),
  params: [{
    name: 'type',
    type: {
      name: 'selection',
      data: ["dictionary", "extension", "locale", "plugin", "theme", "all"]
    },
    defaultValue: 'all',
    description: gcli.lookup("addonListTypeDesc"),
  }],
  exec: function(aArgs, context) {
    function representEnabledAddon(aAddon) {
      return "<li><![CDATA[" + aAddon.name + "\u2002" + aAddon.version +
      getAddonStatus(aAddon) + "]]></li>";
    }

    function representDisabledAddon(aAddon) {
      return "<li class=\"gcli-addon-disabled\">" +
        "<![CDATA[" + aAddon.name + "\u2002" + aAddon.version + aAddon.version +
        "]]></li>";
    }

    function getAddonStatus(aAddon) {
      let operations = [];

      if (aAddon.pendingOperations & AddonManager.PENDING_ENABLE) {
        operations.push("PENDING_ENABLE");
      }

      if (aAddon.pendingOperations & AddonManager.PENDING_DISABLE) {
        operations.push("PENDING_DISABLE");
      }

      if (aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL) {
        operations.push("PENDING_UNINSTALL");
      }

      if (aAddon.pendingOperations & AddonManager.PENDING_INSTALL) {
        operations.push("PENDING_INSTALL");
      }

      if (aAddon.pendingOperations & AddonManager.PENDING_UPGRADE) {
        operations.push("PENDING_UPGRADE");
      }

      if (operations.length) {
        return " (" + operations.join(", ") + ")";
      }
      return "";
    }

    /**
     * Compares two addons by their name. Used in sorting.
     */
    function compareAddonNames(aNameA, aNameB) {
      return String.localeCompare(aNameA.name, aNameB.name);
    }

    /**
     * Resolves the promise which is the scope (this) of this function, filling
     * it with an HTML representation of the passed add-ons.
     */
    function list(aType, aAddons) {
      if (!aAddons.length) {
        this.resolve(gcli.lookup("addonNoneOfType"));
      }

      // Separate the enabled add-ons from the disabled ones.
      let enabledAddons = [];
      let disabledAddons = [];

      aAddons.forEach(function(aAddon) {
        if (aAddon.isActive) {
          enabledAddons.push(aAddon);
        } else {
          disabledAddons.push(aAddon);
        }
      });

      let header;
      switch(aType) {
        case "dictionary":
          header = gcli.lookup("addonListDictionaryHeading");
          break;
        case "extension":
          header = gcli.lookup("addonListExtensionHeading");
          break;
        case "locale":
          header = gcli.lookup("addonListLocaleHeading");
          break;
        case "plugin":
          header = gcli.lookup("addonListPluginHeading");
          break;
        case "theme":
          header = gcli.lookup("addonListThemeHeading");
        case "all":
          header = gcli.lookup("addonListAllHeading");
          break;
        default:
          header = gcli.lookup("addonListUnknownHeading");
      }

      // Map and sort the add-ons, and create an HTML list.
      this.resolve(header +
        "<ol>" +
        enabledAddons.sort(compareAddonNames).map(representEnabledAddon).join("") +
        disabledAddons.sort(compareAddonNames).map(representDisabledAddon).join("") +
        "</ol>");
    }

    // Create the promise that will be resolved when the add-on listing has
    // been finished.
    let promise = context.createPromise();
    let types = aArgs.type == "all" ? null : [aArgs.type];
    AddonManager.getAddonsByTypes(types, list.bind(promise, aArgs.type));
    return promise;
  }
});


/**
 * 'dbg' command
 */
gcli.addCommand({
  name: "dbg",
  description: gcli.lookup("dbgDesc"),
  manual: gcli.lookup("dbgManual")
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

// We need a list of addon names for the enable and disable commands. Because
// getting the name list is async we do not add the commands until we have the
// list.
AddonManager.getAllAddons(function addonAsync(aAddons) {
  // We listen for installs to keep our addon list up to date. There is no need
  // to listen for uninstalls because uninstalled addons are simply disabled
  // until restart (to enable undo functionality).
  AddonManager.addAddonListener({
    onInstalled: function(aAddon) {
      addonNameCache.push({
        name: representAddon(aAddon).replace(/\s/g, "_"),
        value: aAddon.name
      });
    },
    onUninstalled: function(aAddon) {
      let name = representAddon(aAddon).replace(/\s/g, "_");

      for (let i = 0; i < addonNameCache.length; i++) {
        if(addonNameCache[i].name == name) {
          addonNameCache.splice(i, 1);
          break;
        }
      }
    },
  });

  /**
   * Returns a string that represents the passed add-on.
   */
  function representAddon(aAddon) {
    let name = aAddon.name + " " + aAddon.version;
    return name.trim();
  }

  let addonNameCache = [];

  // The name parameter, used in "addon enable" and "addon disable."
  let nameParameter = {
    name: "name",
    type: {
      name: "selection",
      lookup: addonNameCache
    },
    description: gcli.lookup("addonNameDesc")
  };

  for (let addon of aAddons) {
    addonNameCache.push({
      name: representAddon(addon).replace(/\s/g, "_"),
      value: addon.name
    });
  }

  /**
   * 'addon enable' command.
   */
  gcli.addCommand({
    name: "addon enable",
    description: gcli.lookup("addonEnableDesc"),
    params: [nameParameter],
    exec: function(aArgs, context) {
      /**
       * Enables the addon in the passed list which has a name that matches
       * according to the passed name comparer, and resolves the promise which
       * is the scope (this) of this function to display the result of this
       * enable attempt.
       */
      function enable(aName, addons) {
        // Find the add-on.
        let addon = null;
        addons.some(function(candidate) {
          if (candidate.name == aName) {
            addon = candidate;
            return true;
          } else {
            return false;
          }
        });

        let name = representAddon(addon);

        if (!addon.userDisabled) {
          this.resolve("<![CDATA[" +
            gcli.lookupFormat("addonAlreadyEnabled", [name]) + "]]>");
        } else {
          addon.userDisabled = false;
          // nl-nl: {$1} is ingeschakeld.
          this.resolve("<![CDATA[" +
            gcli.lookupFormat("addonEnabled", [name]) + "]]>");
        }
      }

      let promise = context.createPromise();
      // List the installed add-ons, enable one when done listing.
      AddonManager.getAllAddons(enable.bind(promise, aArgs.name));
      return promise;
    }
  });

  /**
   * 'addon disable' command.
   */
  gcli.addCommand({
    name: "addon disable",
    description: gcli.lookup("addonDisableDesc"),
    params: [nameParameter],
    exec: function(aArgs, context) {
      /**
       * Like enable, but ... you know ... the exact opposite.
       */
      function disable(aName, addons) {
        // Find the add-on.
        let addon = null;
        addons.some(function(candidate) {
          if (candidate.name == aName) {
            addon = candidate;
            return true;
          } else {
            return false;
          }
        });

        let name = representAddon(addon);

        if (addon.userDisabled) {
          this.resolve("<![CDATA[" +
            gcli.lookupFormat("addonAlreadyDisabled", [name]) + "]]>");
        } else {
          addon.userDisabled = true;
          // nl-nl: {$1} is uitgeschakeld.
          this.resolve("<![CDATA[" +
            gcli.lookupFormat("addonDisabled", [name]) + "]]>");
        }
      }

      let promise = context.createPromise();
      // List the installed add-ons, disable one when done listing.
      AddonManager.getAllAddons(disable.bind(promise, aArgs.name));
      return promise;
    }
  });
  Services.obs.notifyObservers(null, "gcli_addon_commands_ready", null);
});

/* Responsive Mode commands */
(function gcli_cmd_resize_container() {
  function gcli_cmd_resize(args, context) {
    let browserDoc = context.environment.chromeDocument;
    let browserWindow = browserDoc.defaultView;
    let mgr = browserWindow.ResponsiveUI.ResponsiveUIManager;
    mgr.handleGcliCommand(browserWindow,
                          browserWindow.gBrowser.selectedTab,
                          this.name,
                          args);
  }

  gcli.addCommand({
    name: 'resize',
    description: gcli.lookup('resizeModeDesc')
  });

  gcli.addCommand({
    name: 'resize on',
    description: gcli.lookup('resizeModeOnDesc'),
    manual: gcli.lookup('resizeModeManual'),
    exec: gcli_cmd_resize
  });

  gcli.addCommand({
    name: 'resize off',
    description: gcli.lookup('resizeModeOffDesc'),
    manual: gcli.lookup('resizeModeManual'),
    exec: gcli_cmd_resize
  });

  gcli.addCommand({
    name: 'resize toggle',
    description: gcli.lookup('resizeModeToggleDesc'),
    manual: gcli.lookup('resizeModeManual'),
    exec: gcli_cmd_resize
  });

  gcli.addCommand({
    name: 'resize to',
    description: gcli.lookup('resizeModeToDesc'),
    params: [
      {
        name: 'width',
        type: 'number',
        description: gcli.lookup("resizePageArgWidthDesc"),
      },
      {
        name: 'height',
        type: 'number',
        description: gcli.lookup("resizePageArgHeightDesc"),
      },
    ],
    exec: gcli_cmd_resize
  });
})();

/**
 * jsb command.
 */
gcli.addCommand({
  name: 'jsb',
  description: gcli.lookup('jsbDesc'),
  returnValue:'string',
  hidden: true,
  params: [
    {
      name: 'url',
      type: 'string',
      description: gcli.lookup('jsbUrlDesc'),
      manual: 'The URL of the JS to prettify'
    },
    {
      name: 'indentSize',
      type: 'number',
      description: gcli.lookup('jsbIndentSizeDesc'),
      manual: gcli.lookup('jsbIndentSizeManual'),
      defaultValue: 2
    },
    {
      name: 'indentChar',
      type: {
        name: 'selection',
        lookup: [{name: "space", value: " "}, {name: "tab", value: "\t"}]
      },
      description: gcli.lookup('jsbIndentCharDesc'),
      manual: gcli.lookup('jsbIndentCharManual'),
      defaultValue: ' ',
    },
    {
      name: 'preserveNewlines',
      type: 'boolean',
      description: gcli.lookup('jsbPreserveNewlinesDesc'),
      manual: gcli.lookup('jsbPreserveNewlinesManual'),
      defaultValue: true
    },
    {
      name: 'preserveMaxNewlines',
      type: 'number',
      description: gcli.lookup('jsbPreserveMaxNewlinesDesc'),
      manual: gcli.lookup('jsbPreserveMaxNewlinesManual'),
      defaultValue: -1
    },
    {
      name: 'jslintHappy',
      type: 'boolean',
      description: gcli.lookup('jsbJslintHappyDesc'),
      manual: gcli.lookup('jsbJslintHappyManual'),
      defaultValue: false
    },
    {
      name: 'braceStyle',
      type: {
        name: 'selection',
        data: ['collapse', 'expand', 'end-expand', 'expand-strict']
      },
      description: gcli.lookup('jsbBraceStyleDesc'),
      manual: gcli.lookup('jsbBraceStyleManual'),
      defaultValue: "collapse"
    },
    {
      name: 'spaceBeforeConditional',
      type: 'boolean',
      description: gcli.lookup('jsbSpaceBeforeConditionalDesc'),
      manual: gcli.lookup('jsbSpaceBeforeConditionalManual'),
      defaultValue: true
    },
    {
      name: 'unescapeStrings',
      type: 'boolean',
      description: gcli.lookup('jsbUnescapeStringsDesc'),
      manual: gcli.lookup('jsbUnescapeStringsManual'),
      defaultValue: false
    }
  ],
  exec: function(args, context) {
  let opts = {
    indent_size: args.indentSize,
    indent_char: args.indentChar,
    preserve_newlines: args.preserveNewlines,
    max_preserve_newlines: args.preserveMaxNewlines == -1 ?
                           undefined : args.preserveMaxNewlines,
    jslint_happy: args.jslintHappy,
    brace_style: args.braceStyle,
    space_before_conditional: args.spaceBeforeConditional,
    unescape_strings: args.unescapeStrings
  }

  let xhr = new XMLHttpRequest();

  try {
    xhr.open("GET", args.url, true);
  } catch(e) {
    return gcli.lookup('jsbInvalidURL');
  }

  let promise = context.createPromise();

  xhr.onreadystatechange = function(aEvt) {
    if (xhr.readyState == 4) {
      if (xhr.status == 200 || xhr.status == 0) {
        let browserDoc = context.environment.chromeDocument;
        let browserWindow = browserDoc.defaultView;
        let browser = browserWindow.gBrowser;

        browser.selectedTab = browser.addTab("data:text/plain;base64," +
          browserWindow.btoa(js_beautify(xhr.responseText, opts)));
        promise.resolve();
      }
      else {
        promise.resolve("Unable to load page to beautify: " + args.url + " " +
                        xhr.status + " " + xhr.statusText);
      }
    };
  }
  xhr.send(null);
  return promise;
  }
});
