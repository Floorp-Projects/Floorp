/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

let tempScope = {};
Cu.import("resource:///modules/devtools/Target.jsm", tempScope);
let TargetFactory = tempScope.TargetFactory;
let doc = null, toolbox = null, panelWin = null, index = 0, prefValues = [], prefNodes = [];

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  let target = TargetFactory.forTab(gBrowser.selectedTab);

  gBrowser.selectedBrowser.addEventListener("load", function onLoad(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, onLoad, true);
    gDevTools.showToolbox(target).then(testSelectTool);
  }, true);

  content.location = "data:text/html,test for dynamically registering and unregistering tools";
}

function testSelectTool(aToolbox) {
  toolbox = aToolbox;
  doc = toolbox.doc;
  toolbox.once("options-selected", testOptionsShortcut);
  toolbox.selectTool("options");
}

function testOptionsShortcut() {
  ok(true, "Toolbox selected via selectTool method");
  toolbox.once("options-selected", testOptionsButtonClick);
  toolbox.selectTool("webconsole")
         .then(() => synthesizeKeyFromKeyTag("toolbox-options-key", doc));
}

function testOptionsButtonClick() {
  ok(true, "Toolbox selected via shortcut");
  toolbox.once("options-selected", testOptions);
  toolbox.selectTool("webconsole")
         .then(() => doc.getElementById("toolbox-tab-options").click());
}

function testOptions(event, iframe) {
  ok(true, "Toolbox selected via button click");
  panelWin = iframe.contentWindow;
  let panelDoc = iframe.contentDocument;
  // Testing pref changes
  let prefCheckboxes = panelDoc.querySelectorAll("checkbox[data-pref]");
  for (let checkbox of prefCheckboxes) {
    prefNodes.push(checkbox);
    prefValues.push(Services.prefs.getBoolPref(checkbox.getAttribute("data-pref")));
  }
  // Do again with opposite values to reset prefs
  for (let checkbox of prefCheckboxes) {
    prefNodes.push(checkbox);
    prefValues.push(!Services.prefs.getBoolPref(checkbox.getAttribute("data-pref")));
  }
  testMouseClicks();
}

function testMouseClicks() {
  if (index == prefValues.length) {
    checkTools();
    return;
  }
  gDevTools.once("pref-changed", prefChanged);
  info("Click event synthesized for index " + index);
  EventUtils.synthesizeMouse(prefNodes[index], 10, 10, {}, panelWin);
}

function prefChanged(event, data) {
  if (data.pref == prefNodes[index].getAttribute("data-pref")) {
    ok(true, "Correct pref was changed");
    is(data.oldValue, prefValues[index], "Previous value is correct");
    is(data.newValue, !prefValues[index], "New value is correct");
    index++;
    testMouseClicks();
    return;
  }
  ok(false, "Pref was not changed correctly");
  cleanup();
}

function checkTools() {
  let toolsPref = panelWin.document.querySelectorAll("#default-tools-box > checkbox");
  prefNodes = [];
  index = 0;
  for (let tool of toolsPref) {
    prefNodes.push(tool);
  }
  // Wait for the next turn of the event loop to avoid stack overflow errors.
  executeSoon(toggleTools);
}

function toggleTools() {
  if (index < prefNodes.length) {
    gDevTools.once("tool-unregistered", checkUnregistered);
    EventUtils.synthesizeMouse(prefNodes[index], 10, 10, {}, panelWin);
  }
  else if (index < 2*prefNodes.length) {
    gDevTools.once("tool-registered", checkRegistered);
    EventUtils.synthesizeMouse(prefNodes[index - prefNodes.length], 10, 10, {}, panelWin);
  }
  else {
    cleanup();
    return;
  }
}

function checkUnregistered(event, data) {
  if (data == prefNodes[index].getAttribute("id")) {
    ok(true, "Correct tool removed");
    // checking tab on the toolbox
    ok(!doc.getElementById("toolbox-tab-" + data), "Tab removed for " + data);
    index++;
    // Wait for the next turn of the event loop to avoid stack overflow errors.
    executeSoon(toggleTools);
    return;
  }
  ok(false, "Something went wrong, " + data + " was not unregistered");
  cleanup();
}

function checkRegistered(event, data) {
  if (data == prefNodes[index - prefNodes.length].getAttribute("id")) {
    ok(true, "Correct tool added back");
    // checking tab on the toolbox
    ok(doc.getElementById("toolbox-tab-" + data), "Tab added back for " + data);
    index++;
    // Wait for the next turn of the event loop to avoid stack overflow errors.
    executeSoon(toggleTools);
    return;
  }
  ok(false, "Something went wrong, " + data + " was not registered back");
  cleanup();
}

function cleanup() {
  toolbox.destroy().then(function() {
    gBrowser.removeCurrentTab();
    toolbox = doc = prefNodes = prefValues = panelWin = null;
    finish();
  });
}
