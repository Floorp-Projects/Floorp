/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

this.EXPORTED_SYMBOLS = [ ];

Cu.import("resource:///modules/devtools/gcli.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "gDevTools",
                                  "resource:///modules/devtools/gDevTools.jsm");

XPCOMUtils.defineLazyGetter(this, "Debugger", function() {
  let JsDebugger = {};
  Components.utils.import("resource://gre/modules/jsdebugger.jsm", JsDebugger);

  let global = Components.utils.getGlobalForObject({});
  JsDebugger.addDebuggerToGlobal(global);

  return global.Debugger;
});

XPCOMUtils.defineLazyModuleGetter(this, "TargetFactory",
                                  "resource:///modules/devtools/Target.jsm");

let debuggers = [];

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

    debuggers.push(dbg);

    let gBrowser = context.environment.chromeDocument.defaultView.gBrowser;
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    gDevTools.showToolbox(target, "webconsole");

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
    let numDebuggers = debuggers.length;
    if (numDebuggers == 0) {
      return gcli.lookup("calllogStopNoLogging");
    }

    for (let dbg of debuggers) {
      dbg.onEnterFrame = undefined;
    }
    debuggers = [];

    return gcli.lookupFormat("calllogStopReply", [ numDebuggers ]);
  }
});
