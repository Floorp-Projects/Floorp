/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Cu } = require("chrome");
module.exports = [];

var gcli = require('gcli/index');

loader.lazyGetter(this, "gDevTools",
  () => Cu.import("resource:///modules/devtools/gDevTools.jsm", {}).gDevTools);

var { Promise: promise } = Cu.import("resource://gre/modules/Promise.jsm", {});

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
  params: [],

  exec: function (args, context) {
    function start() {
      let panel = getPanel(context, "jsprofiler");

      if (panel.recordingProfile)
        throw gcli.lookup("profilerAlreadyStarted2");

      panel.toggleRecording();
      return gcli.lookup("profilerStarted2");
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
  params: [],

  exec: function (args, context) {
    function stop() {
      let panel = getPanel(context, "jsprofiler");

      if (!panel.recordingProfile)
        throw gcli.lookup("profilerNotStarted3");

      panel.toggleRecording();
      return gcli.lookup("profilerStopped");
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
