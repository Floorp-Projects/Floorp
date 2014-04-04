/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let doc = null, toolbox = null, panelWin = null, modifiedPrefs = [];

function test() {
  waitForExplicitFinish();

  const URL = "data:text/html;charset=utf8,test for dynamically registering and unregistering tools";
  Task.spawn(function* () {
    let { target } = yield addTab(URL);
    let toolbox = yield gDevTools.showToolbox(target);
    yield testSelectTool(toolbox);
    yield testOptionsShortcut();
    yield testOptions();
    yield testToggleTools();
  }).then(cleanup, errorHandler);
}

function testSelectTool(aToolbox) {
  let deferred = promise.defer();

  toolbox = aToolbox;
  doc = toolbox.doc;
  toolbox.once("options-selected", () => {
    ok(true, "Toolbox selected via selectTool method");
    deferred.resolve();
  });
  toolbox.selectTool("options");

  return deferred.promise;
}

function testOptionsShortcut() {
  let deferred = promise.defer();

  toolbox.selectTool("webconsole")
         .then(() => synthesizeKeyFromKeyTag("toolbox-options-key", doc))
         .then(() => {
           ok(true, "Toolbox selected via shortcut key");
           deferred.resolve();
         });

  return deferred.promise;
}

function testOptions() {
  let tool = toolbox.getPanel("options");
  panelWin = tool.panelWin;
  let prefNodes = tool.panelDoc.querySelectorAll("checkbox[data-pref]");

  // Store modified pref names so that they can be cleared on error.
  for (let node of prefNodes) {
    let pref = node.getAttribute("data-pref");
    modifiedPrefs.push(pref);
  }

  // Test each options pref
  let p = promise.resolve();
  for (let node of prefNodes) {
    let prefValue = Services.prefs.getBoolPref(node.getAttribute("data-pref"));
    p = p.then(testMouseClick.bind(null, node, prefValue));
  }
  // Do again with opposite values to reset prefs
  for (let node of prefNodes) {
    let prefValue = !Services.prefs.getBoolPref(node.getAttribute("data-pref"));
    p = p.then(testMouseClick.bind(null, node, prefValue));
  }

  return p;
}

function testMouseClick(node, prefValue) {
  let deferred = promise.defer();

  let pref = node.getAttribute("data-pref");
  gDevTools.once("pref-changed", (event, data) => {
    if (data.pref == pref) {
      ok(true, "Correct pref was changed");
      is(data.oldValue, prefValue, "Previous value is correct");
      is(data.newValue, !prefValue, "New value is correct");
    } else {
      ok(false, "Pref " + pref + " was not changed correctly");
    }
    deferred.resolve();
  });

  node.scrollIntoView();

  // We use executeSoon here to ensure that the element is in view and
  // clickable.
  executeSoon(function() {
    info("Click event synthesized for pref " + pref);
    EventUtils.synthesizeMouseAtCenter(node, {}, panelWin);
  });

  return deferred.promise;
}

function testToggleTools() {
  let toolNodes = panelWin.document.querySelectorAll("#default-tools-box > checkbox:not([unsupported])");
  let enabledTools = Array.prototype.filter.call(toolNodes, node => node.checked);

  let toggleableTools = gDevTools.getDefaultTools().filter(tool => tool.visibilityswitch);
  for (let node of toolNodes) {
    let id = node.getAttribute("id");
    ok (toggleableTools.some(tool => tool.id === id),
      "There should be a toggle checkbox for: " + id);
  }

  // Store modified pref names so that they can be cleared on error.
  for (let tool of toggleableTools) {
    let pref = tool.visibilityswitch;
    modifiedPrefs.push(pref);
  }

  // Toggle each tool
  let p = promise.resolve();
  for (let node of toolNodes) {
    p = p.then(toggleTool.bind(null, node));
  }
  // Toggle again to reset tool enablement state
  for (let node of toolNodes) {
    p = p.then(toggleTool.bind(null, node));
  }

  // Test that a tool can still be added when no tabs are present:
  // Disable all tools
  for (let node of enabledTools) {
    p = p.then(toggleTool.bind(null, node));
  }
  // Re-enable the tools which are enabled by default
  for (let node of enabledTools) {
    p = p.then(toggleTool.bind(null, node));
  }

  // Toggle first, middle, and last tools to ensure that toolbox tabs are
  // inserted in order
  let firstTool  = toolNodes[0],
      middleTool = toolNodes[(toolNodes.length / 2) | 0],
      lastTool   = toolNodes[toolNodes.length - 1];

  p = p.then(toggleTool.bind(null, firstTool))
       .then(toggleTool.bind(null, firstTool))
       .then(toggleTool.bind(null, middleTool))
       .then(toggleTool.bind(null, middleTool))
       .then(toggleTool.bind(null, lastTool))
       .then(toggleTool.bind(null, lastTool));

  return p;
}

function toggleTool(node) {
  let deferred = promise.defer();

  let toolId = node.getAttribute("id");
  if (node.checked) {
    gDevTools.once("tool-unregistered", checkUnregistered.bind(null, toolId, deferred));
  } else {
    gDevTools.once("tool-registered", checkRegistered.bind(null, toolId, deferred));
  }
  node.scrollIntoView();
  EventUtils.synthesizeMouseAtCenter(node, {}, panelWin);

  return deferred.promise;
}

function checkUnregistered(toolId, deferred, event, data) {
  if (data.id == toolId) {
    ok(true, "Correct tool removed");
    // checking tab on the toolbox
    ok(!doc.getElementById("toolbox-tab-" + toolId), "Tab removed for " + toolId);
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

function cleanup() {
  toolbox.destroy().then(function() {
    gBrowser.removeCurrentTab();
    for (let pref of modifiedPrefs) {
      Services.prefs.clearUserPref(pref);
    }
    toolbox = doc = panelWin = modifiedPrefs = null;
    finish();
  });
}

function errorHandler(error) {
  ok(false, "Unexpected error: " + error);
  cleanup();
}
