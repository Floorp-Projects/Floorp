let testPage = "data:text/html,<body><input id='input1' value='value'><select size=2><option val=1>One</select></body>";

let browser;

function test() {
  waitForExplicitFinish();

  var tab = gBrowser.addTab();
  browser = gBrowser.getBrowserForTab(tab);
  gBrowser.selectedTab = tab;

  function runFirstTest(event) {
    browser.removeEventListener("load", runFirstTest, true);
    addEventListener("commandupdate", checkTest, false);
    doTest();
  }

  browser.addEventListener("load", runFirstTest, true);
  browser.contentWindow.location = testPage;

  gURLBar.focus();
}

let currentTest;

let tests = [
  // Switch focus to the content. The body should be focused and select all should be enabled. In a
  // multi-process browser, we won't see the blur update.
  { name: "focus body", test: function() { browser.focus() }, skipUpdate: gMultiProcessBrowser ? 0 : 1,
    commands: { "cmd_copy" : false, "cmd_paste": false, "cmd_selectAll" : true, "cmd_undo" : false, "cmd_redo": false } },

  // Switch focus to 'input1'. Paste and select all should be enabled.
  { name: "focus input", test: function() { EventUtils.synthesizeKey("VK_TAB", {}) }, skipUpdate: 2,
    commands: { "cmd_copy" : true, "cmd_paste": true, "cmd_selectAll" : true, "cmd_undo" : false, "cmd_redo": false } },

  // Move cursor to end which will deselect the text. Copy should be disabled but paste and select all should still be enabled.
  { name: "cursor right", test: function() { EventUtils.synthesizeKey("VK_RIGHT", {}) },
    commands: { "cmd_copy" : false, "cmd_paste": true, "cmd_selectAll" : true, "cmd_undo" : false, "cmd_redo": false } },

  // Select all of the text. Copy should become enabled.
  { name: "select all", test: function() { EventUtils.synthesizeKey("a", { accelKey: true }) },
    commands: { "cmd_copy" : true, "cmd_paste": true, "cmd_selectAll" : true, "cmd_undo" : false, "cmd_redo": false } },

  // Replace the text with 'c'. Copy should now be disabled and undo enabled.
  { name: "change value", test: function() { EventUtils.synthesizeKey("c", {}) },
    commands: { "cmd_copy" : false, "cmd_paste": true, "cmd_selectAll" : true, "cmd_undo" : true, "cmd_redo": false } },

  // Undo. Undo should be disabled and redo enabled. The text is reselected so copy is enabled.
  { name: "undo", test: function() { EventUtils.synthesizeKey("z", {accelKey: true }) },
    commands: { "cmd_copy" : true, "cmd_paste": true, "cmd_selectAll" : true, "cmd_undo" : false, "cmd_redo": true  } },

  // Switch focus to the select. Only select all should now be enabled.
  { name: "focus select", test: function() { EventUtils.synthesizeKey("VK_TAB", {}) }, skipUpdate: 1,
    commands: { "cmd_copy" : false, "cmd_paste": false, "cmd_selectAll" : true, "cmd_undo" : false, "cmd_redo": false } },
];

function doTest()
{
  if (!tests.length) {
    removeEventListener("commandupdate", checkTest, false);
    gBrowser.removeCurrentTab();
    finish();
    return;
  }

  currentTest = tests.shift();
  currentTest.test();
}

function checkTest(event)
{
  // Skip events fired on command updaters other than the main edit menu one.
  if (event.target != document.getElementById("editMenuCommandSetAll")) {
    return;
  }

  // Updates also occur on blur. These should be ignored.
  if ("skipUpdate" in currentTest && currentTest["skipUpdate"] > 0) {
    currentTest["skipUpdate"]--;
    return;
  }

  for (let command in currentTest.commands) {
    is(document.getElementById(command).getAttribute("disabled") != "true", currentTest.commands[command],
       currentTest["name"] + " " + command);
  }

  SimpleTest.executeSoon(doTest);
}
