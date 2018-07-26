/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that changing preferences in the options panel updates the prefs
// and toggles appropriate things in the toolbox.

var doc = null, toolbox = null, panelWin = null, modifiedPrefs = [];
const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/client/locales/toolbox.properties");
const {PrefObserver} = require("devtools/client/shared/prefs");

add_task(async function() {
  const URL = "data:text/html;charset=utf8,test for dynamically registering " +
              "and unregistering tools";
  registerNewTool();
  const tab = await addTab(URL);
  const target = TargetFactory.forTab(tab);
  toolbox = await gDevTools.showToolbox(target);

  doc = toolbox.doc;
  await registerNewPerToolboxTool();
  await testSelectTool();
  await testOptionsShortcut();
  await testOptions();
  await testToggleTools();

  // Test that registered WebExtensions becomes entries in the
  // options panel and toggling their checkbox toggle the related
  // preference.
  await registerNewWebExtensions();
  await testToggleWebExtensions();

  await cleanup();
});

function registerNewTool() {
  const toolDefinition = {
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

// Register a fake WebExtension to check that it is
// listed in the toolbox options.
function registerNewWebExtensions() {
  // Register some fake extensions and init the related preferences
  // (similarly to ext-devtools.js).
  for (let i = 0; i < 2; i++) {
    const extPref = `devtools.webextensions.fakeExtId${i}.enabled`;
    Services.prefs.setBoolPref(extPref, true);

    toolbox.registerWebExtension(`fakeUUID${i}`, {
      name: `Fake WebExtension ${i}`,
      pref: extPref,
    });
  }
}

function registerNewPerToolboxTool() {
  const toolDefinition = {
    id: "test-pertoolbox-tool",
    isTargetSupported: () => true,
    visibilityswitch: "devtools.test-pertoolbox-tool.enabled",
    url: "about:blank",
    label: "perToolboxSomeLabel"
  };

  ok(gDevTools, "gDevTools exists");
  ok(!gDevTools.getToolDefinitionMap().has("test-pertoolbox-tool"),
     "The per-toolbox tool is not registered globally");

  ok(toolbox, "toolbox exists");
  ok(!toolbox.hasAdditionalTool("test-pertoolbox-tool"),
     "The per-toolbox tool is not yet registered to the toolbox");

  toolbox.addAdditionalTool(toolDefinition);

  ok(!gDevTools.getToolDefinitionMap().has("test-pertoolbox-tool"),
     "The per-toolbox tool is not registered globally");
  ok(toolbox.hasAdditionalTool("test-pertoolbox-tool"),
     "The per-toolbox tool has been registered to the toolbox");
}

async function testSelectTool() {
  info("Checking to make sure that the options panel can be selected.");

  const onceSelected = toolbox.once("options-selected");
  toolbox.selectTool("options");
  await onceSelected;
  ok(true, "Toolbox selected via selectTool method");
}

async function testOptionsShortcut() {
  info("Selecting another tool, then reselecting options panel with keyboard.");

  await toolbox.selectTool("webconsole");
  is(toolbox.currentToolId, "webconsole", "webconsole is selected");
  synthesizeKeyShortcut(L10N.getStr("toolbox.help.key"));
  is(toolbox.currentToolId, "options", "Toolbox selected via shortcut key");
  synthesizeKeyShortcut(L10N.getStr("toolbox.help.key"));
  is(toolbox.currentToolId, "webconsole", "webconsole is reselected");
  synthesizeKeyShortcut(L10N.getStr("toolbox.help.key"));
  is(toolbox.currentToolId, "options", "Toolbox selected via shortcut key");
}

async function testOptions() {
  const tool = toolbox.getPanel("options");
  panelWin = tool.panelWin;
  const prefNodes = tool.panelDoc.querySelectorAll("input[type=checkbox][data-pref]");

  // Store modified pref names so that they can be cleared on error.
  for (const node of tool.panelDoc.querySelectorAll("[data-pref]")) {
    const pref = node.getAttribute("data-pref");
    modifiedPrefs.push(pref);
  }

  for (const node of prefNodes) {
    const prefValue = GetPref(node.getAttribute("data-pref"));

    // Test clicking the checkbox for each options pref
    await testMouseClick(node, prefValue);

    // Do again with opposite values to reset prefs
    await testMouseClick(node, !prefValue);
  }

  const prefSelects = tool.panelDoc.querySelectorAll("select[data-pref]");
  for (const node of prefSelects) {
    await testSelect(node);
  }
}

async function testSelect(select) {
  const pref = select.getAttribute("data-pref");
  const options = Array.from(select.options);
  info("Checking select for: " + pref);

  is(select.options[select.selectedIndex].value, GetPref(pref),
    "select starts out selected");

  for (const option of options) {
    if (options.indexOf(option) === select.selectedIndex) {
      continue;
    }

    const observer = new PrefObserver("devtools.");

    let changeSeen = false;
    const changeSeenPromise = new Promise(resolve => {
      observer.once(pref, () => {
        changeSeen = true;
        is(GetPref(pref), option.value, "Preference been switched for " + pref);
        resolve();
      });
    });

    select.selectedIndex = options.indexOf(option);
    const changeEvent = new Event("change");
    select.dispatchEvent(changeEvent);

    await changeSeenPromise;

    ok(changeSeen, "Correct pref was changed");
    observer.destroy();
  }
}

async function testMouseClick(node, prefValue) {
  const observer = new PrefObserver("devtools.");

  const pref = node.getAttribute("data-pref");
  let changeSeen = false;
  const changeSeenPromise = new Promise(resolve => {
    observer.once(pref, () => {
      changeSeen = true;
      is(GetPref(pref), !prefValue, "New value is correct for " + pref);
      resolve();
    });
  });

  node.scrollIntoView();

  // We use executeSoon here to ensure that the element is in view and
  // clickable.
  executeSoon(function() {
    info("Click event synthesized for pref " + pref);
    EventUtils.synthesizeMouseAtCenter(node, {}, panelWin);
  });

  await changeSeenPromise;

  ok(changeSeen, "Correct pref was changed");
  observer.destroy();
}

async function testToggleWebExtensions() {
  const disabledExtensions = new Set();
  const toggleableWebExtensions = toolbox.listWebExtensions();

  function toggleWebExtension(node) {
    node.scrollIntoView();
    EventUtils.synthesizeMouseAtCenter(node, {}, panelWin);
  }

  function assertExpectedDisabledExtensions() {
    for (const ext of toggleableWebExtensions) {
      if (disabledExtensions.has(ext)) {
        ok(!toolbox.isWebExtensionEnabled(ext.uuid),
           `The WebExtension "${ext.name}" should be disabled`);
      } else {
        ok(toolbox.isWebExtensionEnabled(ext.uuid),
           `The WebExtension "${ext.name}" should  be enabled`);
      }
    }
  }

  function assertAllExtensionsDisabled() {
    const enabledUUIDs = toggleableWebExtensions
            .filter(ext => toolbox.isWebExtensionEnabled(ext.uuid))
            .map(ext => ext.uuid);

    Assert.deepEqual(enabledUUIDs, [],
                     "All the registered WebExtensions should be disabled");
  }

  function assertAllExtensionsEnabled() {
    const disabledUUIDs = toolbox.listWebExtensions()
            .filter(ext => !toolbox.isWebExtensionEnabled(ext.uuid))
            .map(ext => ext.uuid);

    Assert.deepEqual(disabledUUIDs, [],
                     "All the registered WebExtensions should be enabled");
  }

  function getWebExtensionNodes() {
    const toolNodes = panelWin.document.querySelectorAll(
      "#default-tools-box input[type=checkbox]:not([data-unsupported])," +
        "#additional-tools-box input[type=checkbox]:not([data-unsupported])");

    return [...toolNodes].filter(node => {
      return toggleableWebExtensions.some(
        ({uuid}) => node.getAttribute("id") === `webext-${uuid}`
      );
    });
  }

  let webExtensionNodes = getWebExtensionNodes();

  is(webExtensionNodes.length, toggleableWebExtensions.length,
     "There should be a toggle checkbox for every WebExtension registered");

  for (const ext of toggleableWebExtensions) {
    ok(toolbox.isWebExtensionEnabled(ext.uuid),
       `The WebExtension "${ext.name}" is initially enabled`);
  }

  // Store modified pref names so that they can be cleared on error.
  for (const ext of toggleableWebExtensions) {
    modifiedPrefs.push(ext.pref);
  }

  // Turn each registered WebExtension to disabled.
  for (const node of webExtensionNodes) {
    toggleWebExtension(node);

    const toggledExt = toggleableWebExtensions.find(ext => {
      return node.id == `webext-${ext.uuid}`;
    });
    ok(toggledExt, "Found a WebExtension for the checkbox element");
    disabledExtensions.add(toggledExt);

    assertExpectedDisabledExtensions();
  }

  assertAllExtensionsDisabled();

  // Turn each registered WebExtension to enabled.
  for (const node of webExtensionNodes) {
    toggleWebExtension(node);

    const toggledExt = toggleableWebExtensions.find(ext => {
      return node.id == `webext-${ext.uuid}`;
    });
    ok(toggledExt, "Found a WebExtension for the checkbox element");
    disabledExtensions.delete(toggledExt);

    assertExpectedDisabledExtensions();
  }

  assertAllExtensionsEnabled();

  // Unregister the WebExtensions one by one, and check that only the expected
  // ones have been unregistered, and the remaining onea are still listed.
  for (const ext of toggleableWebExtensions) {
    ok(toolbox.listWebExtensions().length > 0,
       "There should still be extensions registered");
    toolbox.unregisterWebExtension(ext.uuid);

    const registeredUUIDs = toolbox.listWebExtensions().map(item => item.uuid);
    ok(!registeredUUIDs.includes(ext.uuid),
       `the WebExtension "${ext.name}" should have been unregistered`);

    webExtensionNodes = getWebExtensionNodes();

    const checkboxEl = webExtensionNodes.find(el => el.id === `webext-${ext.uuid}`);
    is(checkboxEl, undefined,
       "The unregistered WebExtension checkbox should have been removed");

    is(registeredUUIDs.length, webExtensionNodes.length,
       "There should be the expected number of WebExtensions checkboxes");
  }

  is(toolbox.listWebExtensions().length, 0,
     "All WebExtensions have been unregistered");

  webExtensionNodes = getWebExtensionNodes();

  is(webExtensionNodes.length, 0,
     "There should not be any checkbox for the unregistered WebExtensions");
}

function getToolNode(id) {
  return panelWin.document.getElementById(id);
}

async function testToggleTools() {
  const toolNodes = panelWin.document.querySelectorAll(
    "#default-tools-box input[type=checkbox]:not([data-unsupported])," +
    "#additional-tools-box input[type=checkbox]:not([data-unsupported])");
  const toolNodeIds = [...toolNodes].map(node => node.id);
  const enabledToolIds = [...toolNodes].filter(node => node.checked).map(node => node.id);

  const toggleableTools = gDevTools.getDefaultTools()
                                 .filter(tool => {
                                   return tool.visibilityswitch;
                                 })
                                 .concat(gDevTools.getAdditionalTools())
                                 .concat(toolbox.getAdditionalTools());

  for (const node of toolNodes) {
    const id = node.getAttribute("id");
    ok(toggleableTools.some(tool => tool.id === id),
       "There should be a toggle checkbox for: " + id);
  }

  // Store modified pref names so that they can be cleared on error.
  for (const tool of toggleableTools) {
    const pref = tool.visibilityswitch;
    modifiedPrefs.push(pref);
  }

  // Toggle each tool
  for (const id of toolNodeIds) {
    await toggleTool(getToolNode(id));
  }

  // Toggle again to reset tool enablement state
  for (const id of toolNodeIds) {
    await toggleTool(getToolNode(id));
  }

  // Test that a tool can still be added when no tabs are present:
  // Disable all tools
  for (const id of enabledToolIds) {
    await toggleTool(getToolNode(id));
  }
  // Re-enable the tools which are enabled by default
  for (const id of enabledToolIds) {
    await toggleTool(getToolNode(id));
  }

  // Toggle first, middle, and last tools to ensure that toolbox tabs are
  // inserted in order
  const firstToolId = toolNodeIds[0];
  const middleToolId = toolNodeIds[(toolNodeIds.length / 2) | 0];
  const lastToolId = toolNodeIds[toolNodeIds.length - 1];

  await toggleTool(getToolNode(firstToolId));
  await toggleTool(getToolNode(firstToolId));
  await toggleTool(getToolNode(middleToolId));
  await toggleTool(getToolNode(middleToolId));
  await toggleTool(getToolNode(lastToolId));
  await toggleTool(getToolNode(lastToolId));
}

/**
 * Toggle tool node checkbox. Note: because toggling the checkbox will result in
 * re-rendering of the tool list, we must re-query the checkboxes every time.
 */
async function toggleTool(node) {
  const toolId = node.getAttribute("id");

  const registeredPromise = new Promise(resolve => {
    if (node.checked) {
      gDevTools.once("tool-unregistered",
        checkUnregistered.bind(null, toolId, resolve));
    } else {
      gDevTools.once("tool-registered",
        checkRegistered.bind(null, toolId, resolve));
    }
  });
  node.scrollIntoView();
  EventUtils.synthesizeMouseAtCenter(node, {}, panelWin);

  await registeredPromise;
}

function checkUnregistered(toolId, resolve, data) {
  if (data == toolId) {
    ok(true, "Correct tool removed");
    // checking tab on the toolbox
    ok(!doc.getElementById("toolbox-tab-" + toolId),
      "Tab removed for " + toolId);
  } else {
    ok(false, "Something went wrong, " + toolId + " was not unregistered");
  }
  resolve();
}

async function checkRegistered(toolId, resolve, data) {
  if (data == toolId) {
    ok(true, "Correct tool added back");
    // checking tab on the toolbox
    const button = await lookupButtonForToolId(toolId);
    ok(button, "Tab added back for " + toolId);
  } else {
    ok(false, "Something went wrong, " + toolId + " was not registered");
  }
  resolve();
}

function GetPref(name) {
  const type = Services.prefs.getPrefType(name);
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

/**
 * Find the button from specified toolId.
 * Generally, button which access to the tool panel is in toolbox or
 * tools menu(in the Chevron menu).
 */
async function lookupButtonForToolId(toolId) {
  let button = doc.getElementById("toolbox-tab-" + toolId);
  if (!button) {
    // search from the tools menu.
    await openChevronMenu(toolbox);
    button = doc.querySelector("#tools-chevron-menupopup-" + toolId);

    await closeChevronMenu(toolbox);
  }
  return button;
}

async function cleanup() {
  gDevTools.unregisterTool("test-tool");
  await toolbox.destroy();
  gBrowser.removeCurrentTab();
  for (const pref of modifiedPrefs) {
    Services.prefs.clearUserPref(pref);
  }
  toolbox = doc = panelWin = modifiedPrefs = null;
}
