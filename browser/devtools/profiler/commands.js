/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const gcli = require('gcli/index');

loader.lazyGetter(this, "gDevTools",
  () => Cu.import("resource:///modules/devtools/gDevTools.jsm", {}).gDevTools);


module.exports.items = [
{
  name: "profiler",
  description: gcli.lookup("profilerDesc"),
  manual: gcli.lookup("profilerManual")
},
{
  name: "profiler open",
  description: gcli.lookup("profilerOpenDesc"),
  exec: function (args, context) {
    return gDevTools.showToolbox(context.environment.target, "jsprofiler")
      .then(function () null);
  }
},
{
  name: "profiler close",
  description: gcli.lookup("profilerCloseDesc"),
  exec: function (args, context) {
    let toolbox = gDevTools.getToolbox(context.environment.target);
    let panel = (toolbox == null) ? null : toolbox.getPanel(id);
    if (panel == null)
      return;

    return gDevTools.closeToolbox(context.environment.target)
      .then(function () null);
  }
},
{
  name: "profiler start",
  description: gcli.lookup("profilerStartDesc"),
  returnType: "string",
  exec: function (args, context) {
    let target = context.environment.target
    return gDevTools.showToolbox(target, "jsprofiler").then(toolbox => {
      let panel = toolbox.getCurrentPanel();

      if (panel.recordingProfile)
        throw gcli.lookup("profilerAlreadyStarted2");

      panel.toggleRecording();
      return gcli.lookup("profilerStarted2");
    });
  }
},
{
  name: "profiler stop",
  description: gcli.lookup("profilerStopDesc"),
  returnType: "string",
  exec: function (args, context) {
    let target = context.environment.target
    return gDevTools.showToolbox(target, "jsprofiler").then(toolbox => {
      let panel = toolbox.getCurrentPanel();

      if (!panel.recordingProfile)
        throw gcli.lookup("profilerNotStarted3");

      panel.toggleRecording();
      return gcli.lookup("profilerStopped");
    });
  }
},
{
  name: "profiler list",
  description: gcli.lookup("profilerListDesc"),
  returnType: "profileList",
  exec: function (args, context) {
    let toolbox = gDevTools.getToolbox(context.environment.target);
    let panel = (toolbox == null) ? null : toolbox.getPanel("jsprofiler");

    if (panel == null) {
      throw gcli.lookup("profilerNotReady");
    }

    let profileList = [];
    for ([ uid, profile ] of panel.profiles) {
      profileList.push({ name: profile.name, started: profile.isStarted });
    }
    return profileList;
  }
},
{
  item: "converter",
  from: "profileList",
  to: "view",
  exec: function(profileList, context) {
    return {
      html: "<div>" +
            "  <ol>" +
            "    <li forEach='profile of ${profiles}'>${profile.name}</li>" +
            "      ${profile.name} ${profile.started ? '*' : ''}" +
            "    </li>" +
            "  </ol>" +
            "</div>",
      data: { profiles: profileList.profiles },
      options: { allowEval: true }
    };
  },
},
{
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
    let toolbox = gDevTools.getToolbox(context.environment.target);
    let panel = (toolbox == null) ? null : toolbox.getPanel(id);

    if (!panel) {
      throw gcli.lookup("profilerNotReady");
    }

    let profile = panel.getProfileByName(args.name);
    if (!profile) {
      throw gcli.lookup("profilerNotFound");
    }

    panel.sidebar.selectedItem = panel.sidebar.getItemByProfile(profile);
  }
}];
