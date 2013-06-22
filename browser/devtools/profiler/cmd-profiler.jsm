/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
this.EXPORTED_SYMBOLS = [];

Cu.import("resource://gre/modules/devtools/gcli.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/devtools/Require.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "gDevTools",
  "resource:///modules/devtools/gDevTools.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "console",
  "resource://gre/modules/devtools/Console.jsm");

var Promise = require('util/promise');

/*
 * 'profiler' command. Doesn't do anything.
 */
gcli.addCommand({
  name: "profiler",
  description: gcli.lookup("profilerDesc"),
  manual: gcli.lookup("profilerManual")
});

/*
 * 'profiler open' command
 */
gcli.addCommand({
  name: "profiler open",
  description: gcli.lookup("profilerOpenDesc"),
  params: [],

  exec: function (args, context) {
    return gDevTools.showToolbox(context.environment.target, "jsprofiler")
      .then(function () null);
  }
});

/*
 * 'profiler close' command
 */
gcli.addCommand({
  name: "profiler close",
  description: gcli.lookup("profilerCloseDesc"),
  params: [],

  exec: function (args, context) {
    if (!getPanel(context, "jsprofiler"))
      return;

    return gDevTools.closeToolbox(context.environment.target)
      .then(function () null);
  }
});

/*
 * 'profiler start' command
 */
gcli.addCommand({
  name: "profiler start",
  description: gcli.lookup("profilerStartDesc"),
  returnType: "string",

  params: [
    {
      name: "name",
      type: "string",
      manual: gcli.lookup("profilerStartManual")
    }
  ],

  exec: function (args, context) {
    function start() {
      let name = args.name;
      let panel = getPanel(context, "jsprofiler");
      let profile = panel.getProfileByName(name) || panel.createProfile(name);

      if (profile.isStarted) {
        throw gcli.lookup("profilerAlreadyStarted");
      }

      if (profile.isFinished) {
        throw gcli.lookup("profilerAlreadyFinished");
      }

      let item = panel.sidebar.getItemByProfile(profile);

      if (panel.sidebar.selectedItem === item) {
        profile.start();
      } else {
        panel.on("profileSwitched", () => profile.start());
        panel.sidebar.selectedItem = item;
      }

      return gcli.lookup("profilerStarting2");
    }

    return gDevTools.showToolbox(context.environment.target, "jsprofiler")
      .then(start);
  }
});

/*
 * 'profiler stop' command
 */
gcli.addCommand({
  name: "profiler stop",
  description: gcli.lookup("profilerStopDesc"),
  returnType: "string",

  params: [
    {
      name: "name",
      type: "string",
      manual: gcli.lookup("profilerStopManual")
    }
  ],

  exec: function (args, context) {
    function stop() {
      let panel = getPanel(context, "jsprofiler");
      let profile = panel.getProfileByName(args.name);

      if (!profile) {
        throw gcli.lookup("profilerNotFound");
      }

      if (profile.isFinished) {
        throw gcli.lookup("profilerAlreadyFinished");
      }

      if (!profile.isStarted) {
        throw gcli.lookup("profilerNotStarted2");
      }

      let item = panel.sidebar.getItemByProfile(profile);

      if (panel.sidebar.selectedItem === item) {
        profile.stop();
      } else {
        panel.on("profileSwitched", () => profile.stop());
        panel.sidebar.selectedItem = item;
      }

      return gcli.lookup("profilerStopping2");
    }

    return gDevTools.showToolbox(context.environment.target, "jsprofiler")
      .then(stop);
  }
});

/*
 * 'profiler list' command
 */
gcli.addCommand({
  name: "profiler list",
  description: gcli.lookup("profilerListDesc"),
  returnType: "dom",
  params: [],

  exec: function (args, context) {
    let panel = getPanel(context, "jsprofiler");

    if (!panel) {
      throw gcli.lookup("profilerNotReady");
    }

    let doc = panel.document;
    let div = createXHTMLElement(doc, "div");
    let ol = createXHTMLElement(doc, "ol");

    for ([ uid, profile] of panel.profiles) {
      let li = createXHTMLElement(doc, "li");
      li.textContent = profile.name;
      if (profile.isStarted) {
        li.textContent += " *";
      }
      ol.appendChild(li);
    }

    div.appendChild(ol);
    return div;
  }
});

/*
 * 'profiler show' command
 */
gcli.addCommand({
  name: "profiler show",
  description: gcli.lookup("profilerShowDesc"),

  params: [
    {
      name: "name",
      type: "string",
      manual: gcli.lookup("profilerShowManual")
    }
  ],

  exec: function (args, context) {
    let panel = getPanel(context, "jsprofiler");

    if (!panel) {
      throw gcli.lookup("profilerNotReady");
    }

    let profile = panel.getProfileByName(args.name);
    if (!profile) {
      throw gcli.lookup("profilerNotFound");
    }

    panel.sidebar.selectedItem = panel.sidebar.getItemByProfile(profile);
  }
});

function getPanel(context, id) {
  if (context == null) {
    return undefined;
  }

  let toolbox = gDevTools.getToolbox(context.environment.target);
  return toolbox == null ? undefined : toolbox.getPanel(id);
}

function createXHTMLElement(document, tagname) {
  return document.createElementNS("http://www.w3.org/1999/xhtml", tagname);
}