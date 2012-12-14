/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


this.EXPORTED_SYMBOLS = [ ];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource:///modules/devtools/gcli.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "gDevTools",
                                  "resource:///modules/devtools/gDevTools.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TargetFactory",
                                  "resource:///modules/devtools/Target.jsm");

XPCOMUtils.defineLazyGetter(this, "Debugger", function() {
  let JsDebugger = {};
  Cu.import("resource://gre/modules/jsdebugger.jsm", JsDebugger);

  let global = Components.utils.getGlobalForObject({});
  JsDebugger.addDebuggerToGlobal(global);

  return global.Debugger;
});

let debuggers = [];
let sandboxes = [];

/**
 * 'calllog chromestart' command
 */
gcli.addCommand({
  name: "calllog chromestart",
  description: gcli.lookup("calllogChromeStartDesc"),
  get hidden() gcli.hiddenByChromePref(),
  params: [
    {
      name: "sourceType",
      type: {
        name: "selection",
        data: ["content-variable", "chrome-variable", "jsm", "javascript"]
      }
    },
    {
      name: "source",
      type: "string",
      description: gcli.lookup("calllogChromeSourceTypeDesc"),
      manual: gcli.lookup("calllogChromeSourceTypeManual"),
    }
  ],
  exec: function(args, context) {
    let globalObj;
    let contentWindow = context.environment.contentDocument.defaultView;

    if (args.sourceType == "jsm") {
      try {
        globalObj = Cu.import(args.source);
      }
      catch (e) {
        return gcli.lookup("callLogChromeInvalidJSM");
      }
    } else if (args.sourceType == "content-variable") {
      if (args.source in contentWindow) {
        globalObj = Cu.getGlobalForObject(contentWindow[args.source]);
      } else {
        throw new Error(gcli.lookup("callLogChromeVarNotFoundContent"));
      }
    } else if (args.sourceType == "chrome-variable") {
      let chromeWin = context.environment.chromeDocument.defaultView;
      if (args.source in chromeWin) {
        globalObj = Cu.getGlobalForObject(chromeWin[args.source]);
      } else {
        return gcli.lookup("callLogChromeVarNotFoundChrome");
      }
    } else {
      let chromeWin = context.environment.chromeDocument.defaultView;
      let sandbox = new Cu.Sandbox(chromeWin,
                                   {
                                     sandboxPrototype: chromeWin,
                                     wantXrays: false,
                                     sandboxName: "gcli-cmd-calllog-chrome"
                                   });
      let returnVal;
      try {
        returnVal = Cu.evalInSandbox(args.source, sandbox, "ECMAv5");
        sandboxes.push(sandbox);
      } catch(e) {
        // We need to save the message before cleaning up else e contains a dead
        // object.
        let msg = gcli.lookup("callLogChromeEvalException") + ": " + e;
        Cu.nukeSandbox(sandbox);
        return msg;
      }

      if (typeof returnVal == "undefined") {
        return gcli.lookup("callLogChromeEvalNeedsObject");
      }

      globalObj = Cu.getGlobalForObject(returnVal);
    }

    let dbg = new Debugger(globalObj);
    debuggers.push(dbg);

    dbg.onEnterFrame = function(frame) {
      // BUG 773652 -  Make the output from the GCLI calllog command nicer
      contentWindow.console.log(gcli.lookup("callLogChromeMethodCall") +
                                ": " + this.callDescription(frame));
    }.bind(this);

    let gBrowser = context.environment.chromeDocument.defaultView.gBrowser;
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    gDevTools.showToolbox(target, "webconsole");

    return gcli.lookup("calllogChromeStartReply");
  },

  valueToString: function(value) {
    if (typeof value !== "object" || value === null)
      return uneval(value);
    return "[object " + value.class + "]";
  },

  callDescription: function(frame) {
    let name = frame.callee.name || gcli.lookup("callLogChromeAnonFunction");
    let args = frame.arguments.map(this.valueToString).join(", ");
    return name + "(" + args + ")";
  }
});

/**
 * 'calllog chromestop' command
 */
gcli.addCommand({
  name: "calllog chromestop",
  description: gcli.lookup("calllogChromeStopDesc"),
  get hidden() gcli.hiddenByChromePref(),
  exec: function(args, context) {
    let numDebuggers = debuggers.length;
    if (numDebuggers == 0) {
      return gcli.lookup("calllogChromeStopNoLogging");
    }

    for (let dbg of debuggers) {
      dbg.onEnterFrame = undefined;
      dbg.enabled = false;
    }
    for (let sandbox of sandboxes) {
      Cu.nukeSandbox(sandbox);
    }
    debuggers = [];
    sandboxes = [];

    return gcli.lookupFormat("calllogChromeStopReply", [ numDebuggers ]);
  }
});
