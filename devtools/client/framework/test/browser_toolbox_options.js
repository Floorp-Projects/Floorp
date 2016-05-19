/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from shared-head.js */
"use strict";

// Tests that changing preferences in the options panel updates the prefs
// and toggles appropriate things in the toolbox.

var doc = null, toolbox = null, panelWin = null, modifiedPrefs = [];

add_task(function* () {
  const URL = "data:text/html;charset=utf8,test for dynamically registering " +
              "and unregistering tools";
  registerNewTool();
  let tab = yield addTab(URL);
  let target = TargetFactory.forTab(tab);
  toolbox = yield gDevTools.showToolbox(target);
  doc = toolbox.doc;
  yield testSelectTool();
  yield testOptionsShortcut();
  yield testOptions();
  yield testToggleTools();
  yield cleanup();
});

function registerNewTool() {
  let toolDefinition = {
    id: "test-tool",
    isTargetSupported: () => true,
    visibilityswitch: "devtools.test-tool.enabled",
    url: "about:blank",
    label: "someLabel"
  };

  ok(gDevTools, "gDevTools exists");
  ok(!gDevTools.getToolDefinitionMap().has("test-tool"),
    "The tool is not registered");

  gDevTools.registerTool(toolDefinition);
  ok(gDevTools.getToolDefinitionMap().has("test-tool"),
    "The tool is registered");
}

function* testSelectTool() {
  info("Checking to make sure that the options panel can be selected.");

  let onceSelected = toolbox.once("options-selected");
  toolbox.selectTool("options");
  yield onceSelected;
  ok(true, "Toolbox selected via selectTool method");
}

function* testOptionsShortcut() {
  info("Selecting another tool, then reselecting options panel with keyboard.");

  yield toolbox.selectTool("webconsole");
  is(toolbox.currentToolId, "webconsole", "webconsole is selected");
  synthesizeKeyFromKeyTag(doc.getElementById("toolbox-options-key"));
  is(toolbox.currentToolId, "options", "Toolbox selected via shortcut key (1)");
  synthesizeKeyFromKeyTag(doc.getElementById("toolbox-options-key"));
  is(toolbox.currentToolId, "webconsole", "webconsole is selected (1)");

  yield toolbox.selectTool("webconsole");
  is(toolbox.currentToolId, "webconsole", "webconsole is selected");
  synthesizeKeyFromKeyTag(doc.getElementById("toolbox-options-key2"));
  is(toolbox.currentToolId, "options", "Toolbox selected via shortcut key (2)");
  synthesizeKeyFromKeyTag(doc.getElementById("toolbox-options-key"));
  is(toolbox.currentToolId, "webconsole", "webconsole is reselected (2)");
  synthesizeKeyFromKeyTag(doc.getElementById("toolbox-options-key2"));
  is(toolbox.currentToolId, "options", "Toolbox selected via shortcut key (2)");
}

function* testOptions() {
  let tool = toolbox.getPanel("options");
  panelWin = tool.panelWin;
  let prefNodes = tool.panelDoc.querySelectorAll(
    "input[type=checkbox][data-pref]");

  // Store modified pref names so that they can be cleared on error.
  for (let node of tool.panelDoc.querySelectorAll("[data-pref]")) {
    let pref = node.getAttribute("data-pref");
    modifiedPrefs.push(pref);
  }

  for (let node of prefNodes) {
    let prefValue = GetPref(node.getAttribute("data-pref"));

    // Test clicking the checkbox for each options pref
    yield testMouseClick(node, prefValue);

    // Do again with opposite values to reset prefs
    yield testMouseClick(node, !prefValue);
  }

  let prefSelects = tool.panelDoc.querySelectorAll("select[data-pref]");
  for (let node of prefSelects) {
    yield testSelect(node);
  }
}

function* testSelect(select) {
  let pref = select.getAttribute("data-pref");
  let options = Array.from(select.options);
  info("Checking select for: " + pref);

  is(select.options[select.selectedIndex].value, GetPref(pref),
    "select starts out selected");

  for (let option of options) {
    if (options.indexOf(option) === select.selectedIndex) {
      continue;
    }

    let deferred = promise.defer();
    gDevTools.once("pref-changed", (event, data) => {
      if (data.pref == pref) {
        ok(true, "Correct pref was changed");
        is(GetPref(pref), option.value, "Preference been switched for " + pref);
      } else {
        ok(false, "Pref " + pref + " was not changed correctly");
      }
      deferred.resolve();
    });

    select.selectedIndex = options.indexOf(option);
    let changeEvent = new Event("change");
    select.dispatchEvent(changeEvent);

    yield deferred.promise;
  }
}

function* testMouseClick(node, prefValue) {
  let deferred = promise.defer();

  let pref = node.getAttribute("data-pref");
  gDevTools.once("pref-changed", (event, data) => {
    if (data.pref == pref) {
      ok(true, "Correct pref was changed");
      is(data.oldValue, prefValue, "Previous value is correct for " + pref);
      is(data.newValue, !prefValue, "New value is correct for " + pref);
    } else {
      ok(false, "Pref " + pref + " was not changed correctly");
    }
    deferred.resolve();
  });

  node.scrollIntoView();

  // We use executeSoon here to ensure that the element is in view and
  // clickable.
  executeSoon(function () {
    info("Click event synthesized for pref " + pref);
    EventUtils.synthesizeMouseAtCenter(node, {}, panelWin);
  });

  yield deferred.promise;
}

function* testToggleTools() {
  let toolNodes = panelWin.document.querySelectorAll(
    "#default-tools-box input[type=checkbox]:not([data-unsupported])," +
    "#additional-tools-box input[type=checkbox]:not([data-unsupported])");
  let enabledTools = [...toolNodes].filter(node => node.checked);

  let toggleableTools = gDevTools.getDefaultTools().filter(tool => {
    return tool.visibilityswitch;
  }).concat(gDevTools.getAdditionalTools());

  for (let node of toolNodes) {
    let id = node.getAttribute("id");
    ok(toggleableTools.some(tool => tool.id === id),
      "There should be a toggle checkbox for: " + id);
  }

  // Store modified pref names so that they can be cleared on error.
  for (let tool of toggleableTools) {
    let pref = tool.visibilityswitch;
    modifiedPrefs.push(pref);
  }

  // Toggle each tool
  for (let node of toolNodes) {
    yield toggleTool(node);
  }
  // Toggle again to reset tool enablement state
  for (let node of toolNodes) {
    yield toggleTool(node);
  }

  // Test that a tool can still be added when no tabs are present:
  // Disable all tools
  for (let node of enabledTools) {
    yield toggleTool(node);
  }
  // Re-enable the tools which are enabled by default
  for (let node of enabledTools) {
    yield toggleTool(node);
  }

  // Toggle first, middle, and last tools to ensure that toolbox tabs are
  // inserted in order
  let firstTool = toolNodes[0];
  let middleTool = toolNodes[(toolNodes.length / 2) | 0];
  let lastTool = toolNodes[toolNodes.length - 1];

  yield toggleTool(firstTool);
  yield toggleTool(firstTool);
  yield toggleTool(middleTool);
  yield toggleTool(middleTool);
  yield toggleTool(lastTool);
  yield toggleTool(lastTool);
}

function* toggleTool(node) {
  let deferred = promise.defer();

  let toolId = node.getAttribute("id");
  if (node.checked) {
    gDevTools.once("tool-unregistered",
      checkUnregistered.bind(null, toolId, deferred));
  } else {
    gDevTools.once("tool-registered",
      checkRegistered.bind(null, toolId, deferred));
  }
  node.scrollIntoView();
  EventUtils.synthesizeMouseAtCenter(node, {}, panelWin);

  yield deferred.promise;
}

function checkUnregistered(toolId, deferred, event, data) {
  if (data.id == toolId) {
    ok(true, "Correct tool removed");
    // checking tab on the toolbox
    ok(!doc.getElementById("toolbox-tab-" + toolId),
      "Tab removed for " + toolId);
  } else {
    ok(false, "Something went wrong, " + toolId + " was not unregistered");
  }
  deferred.resolve();
}

function checkRegistered(toolId, deferred, event, data) {
  if (data == toolId) {
    ok(true, "Correct tool added back");
    // checking tab on the toolbox
    let radio = doc.getElementById("toolbox-tab-" + toolId);
    ok(radio, "Tab added back for " + toolId);
    if (radio.previousSibling) {
      ok(+radio.getAttribute("ordinal") >=
         +radio.previousSibling.getAttribute("ordinal"),
         "Inserted tab's ordinal is greater than equal to its previous tab." +
         "Expected " + radio.getAttribute("ordinal") + " >= " +
         radio.previousSibling.getAttribute("ordinal"));
    }
    if (radio.nextSibling) {
      ok(+radio.getAttribute("ordinal") <
         +radio.nextSibling.getAttribute("ordinal"),
         "Inserted tab's ordinal is less than its next tab. Expected " +
         radio.getAttribute("ordinal") + " < " +
         radio.nextSibling.getAttribute("ordinal"));
    }
  } else {
    ok(false, "Something went wrong, " + toolId + " was not registered");
  }
  deferred.resolve();
}

function GetPref(name) {
  let type = Services.prefs.getPrefType(name);
  switch (type) {
    case Services.prefs.PREF_STRING:
      return Services.prefs.getCharPref(name);
    case Services.prefs.PREF_INT:
      return Services.prefs.getIntPref(name);
    case Services.prefs.PREF_BOOL:
      return Services.prefs.getBoolPref(name);
    default:
      throw new Error("Unknown type");
  }
}

function* cleanup() {
  gDevTools.unregisterTool("test-tool");
  yield toolbox.destroy();
  gBrowser.removeCurrentTab();
  for (let pref of modifiedPrefs) {
    Services.prefs.clearUserPref(pref);
  }
  toolbox = doc = panelWin = modifiedPrefs = null;
}
