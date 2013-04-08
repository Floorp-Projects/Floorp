/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a test for the Open URL context menu item
// that is shown for network requests
const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-console.html"
const COMMAND_NAME = "consoleCmd_openURL";
const CONTEXT_MENU_ID = "#menu_openURL";

let HUD = null, outputNode = null, contextMenu = null;

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

function consoleOpened(aHud) {
  HUD = aHud;
  outputNode = aHud.outputNode;
  contextMenu = HUD.iframeWindow.document.getElementById("output-contextmenu");

  executeSoon(testOnNotNetActivity);
}

function testOnNotNetActivity() {
  HUD.jsterm.clearOutput();

  outputNode = HUD.outputNode;
  let console = content.wrappedJSObject.console;
  console.log("bug 764572");

  testOnNotNetActivity_command();
}
function testOnNotNetActivity_command () {
  waitForSuccess({
    name: "show no net activity in console",
    validatorFn: function () {
      return outputNode.textContent.indexOf("bug 764572") > -1;
    },
    successFn: function () {
      outputNode.focus();
      outputNode.selectedItem = outputNode.querySelector(".webconsole-msg-log");

      // check whether the command is disable
      goUpdateCommand(COMMAND_NAME);
      let controller = top.document.commandDispatcher.
        getControllerForCommand(COMMAND_NAME);

      let isDisabled = !controller || !controller.isCommandEnabled("consoleCmd_openURL");
      ok(isDisabled, COMMAND_NAME + " should be disabled.");
      executeSoon(testOnNotNetActivity_contextmenu);
    },
    failureFn: testOnNotNetActivity_contextmenu,
  });
}
function testOnNotNetActivity_contextmenu() {
  let target = outputNode.querySelector(".webconsole-msg-log");

  outputNode.focus();
  outputNode.selectedItem = target;

  waitForOpenContextMenu(contextMenu, {
    target: target,
    successFn: function () {
      let isHidden = contextMenu.querySelector(CONTEXT_MENU_ID).hidden;
      ok(isHidden, CONTEXT_MENU_ID + "should be hidden.");

      closeContextMenu(contextMenu);
      executeSoon(testOnNetActivity);
    },
    failureFn: function(){
      closeContextMenu(contextMenu);
      executeSoon(testOnNetActivity);
    },
  });
}

function testOnNetActivity() {
  HUD.jsterm.clearOutput();

  // reload the url to show net activity in console.
  content.location.reload();

  testOnNetActivity_command();
}

function testOnNetActivity_command() {
  waitForSuccess({
    name: "show TEST_URI's net activity in console",
    validatorFn: function () {
      outputNode.focus();
      outputNode.selectedItem = outputNode.querySelector(".webconsole-msg-network");

      let item = outputNode.selectedItem;
      return item && item.url;
    },
    successFn: function () {
      outputNode.focus();

      // set up the event handler for TabOpen
      gBrowser.tabContainer.addEventListener("TabOpen", function onOpen(aEvent) {
        gBrowser.tabContainer.removeEventListener("TabOpen", onOpen, true);

        let tab = aEvent.target;
        onTabOpen(tab);
      }, true);

      // check whether the command is enable
      goUpdateCommand(COMMAND_NAME);
      let controller = top.document.commandDispatcher.
        getControllerForCommand(COMMAND_NAME);
      ok(controller.isCommandEnabled("consoleCmd_openURL"), COMMAND_NAME + " should be enabled.");

      // try to open url.
      goDoCommand(COMMAND_NAME);
    },
    failureFn: testOnNetActivity_contextmenu,
  });
}

// check TabOpen event
function onTabOpen(aTab) {
  waitForSuccess({
    timeout: 10000,
    name: "complete to initialize the opening tab",
    validatorFn: function()
    {
      // wait to complete initialization for the new tab.
      let url = aTab.linkedBrowser.currentURI.spec;
      return url === TEST_URI;
    },
    successFn: function()
    {
      gBrowser.removeTab(aTab);
      executeSoon(testOnNetActivity_contextmenu);
    },
    failureFn: function()
    {
      info("new tab currentURI " + aTab.linkedBrowser.currentURI.spec);
      testOnNetActivity_contextmenu();
    },
  });
}

function testOnNetActivity_contextmenu() {
  let target = outputNode.querySelector(".webconsole-msg-network");

  outputNode.focus();
  outputNode.selectedItem = target;

  waitForOpenContextMenu(contextMenu, {
    target: target,
    successFn: function () {
      let isShown = !contextMenu.querySelector(CONTEXT_MENU_ID).hidden;
      ok(isShown, CONTEXT_MENU_ID + "should be shown.");

      closeContextMenu(contextMenu);
      executeSoon(finalizeTest);
    },
    failureFn: function(){
      closeContextMenu(contextMenu);
      executeSoon(finalizeTest);
    },
  });
}

function finalizeTest() {
  HUD = null;
  outputNode = null;
  contextMenu = null
  finishTest();
}

function closeContextMenu (aContextMenu) {
  aContextMenu.hidePopup();
}
