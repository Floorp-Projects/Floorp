/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var Startup = Cc["@mozilla.org/devtools/startup-clh;1"].getService(Ci.nsISupports)
  .wrappedJSObject;
var {Toolbox} = require("devtools/client/framework/toolbox");

var toolbox, toolIDs, toolShortcuts = [], idIndex, modifiedPrefs = [];

function test() {
  addTab("about:blank").then(function() {
    toolIDs = [];
    for (const [id, definition] of gDevTools._tools) {
      const shortcut = Startup.KeyShortcuts.filter(s => s.toolId == id)[0];
      if (!shortcut) {
        continue;
      }
      toolIDs.push(id);
      toolShortcuts.push(shortcut);

      // Enable disabled tools
      const pref = definition.visibilityswitch;
      if (pref) {
        const prefValue = Services.prefs.getBoolPref(pref, false);
        if (!prefValue) {
          modifiedPrefs.push(pref);
          Services.prefs.setBoolPref(pref, true);
        }
      }
    }
    const target = TargetFactory.forTab(gBrowser.selectedTab);
    idIndex = 0;
    gDevTools.showToolbox(target, toolIDs[0], Toolbox.HostType.WINDOW)
             .then(testShortcuts);
  });
}

function testShortcuts(aToolbox, aIndex) {
  if (aIndex === undefined) {
    aIndex = 1;
  } else if (aIndex == toolIDs.length) {
    tidyUp();
    return;
  }

  toolbox = aToolbox;
  info("Toolbox fired a `ready` event");

  toolbox.once("select", selectCB);

  const shortcut = toolShortcuts[aIndex];
  const key = shortcut.shortcut;
  const toolModifiers = shortcut.modifiers;
  const modifiers = {
    accelKey: toolModifiers.includes("accel"),
    altKey: toolModifiers.includes("alt"),
    shiftKey: toolModifiers.includes("shift"),
  };
  idIndex = aIndex;
  info("Testing shortcut for tool " + aIndex + ":" + toolIDs[aIndex] +
       " using key " + key);
  EventUtils.synthesizeKey(key, modifiers, toolbox.win.parent);
}

function selectCB(id) {
  info("toolbox-select event from " + id);

  is(toolIDs.indexOf(id), idIndex,
     "Correct tool is selected on pressing the shortcut for " + id);

  testShortcuts(toolbox, idIndex + 1);
}

function tidyUp() {
  toolbox.destroy().then(function() {
    gBrowser.removeCurrentTab();

    for (const pref of modifiedPrefs) {
      Services.prefs.clearUserPref(pref);
    }
    toolbox = toolIDs = idIndex = modifiedPrefs = Toolbox = null;
    finish();
  });
}
