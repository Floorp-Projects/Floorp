/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/" +
  "test/test-console.html?_date=" + Date.now();
const COMMAND_NAME = "consoleCmd_copyURL";
const CONTEXT_MENU_ID = "#menu_copyURL";

let HUD = null;
let output = null;
let menu = null;

function test() {
  addTab(TEST_URI);

  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);

    openConsole(null, function (aHud) {
      HUD = aHud;
      output = aHud.outputNode;
      menu = HUD.iframeWindow.document.getElementById("output-contextmenu");

      executeSoon(testWithoutNetActivity);
    });
  }, true);
}

// Return whether "Copy Link Location" command is enabled or not.
function isEnabled() {
  let controller = top.document.commandDispatcher.
    getControllerForCommand(COMMAND_NAME);

  return controller && controller.isCommandEnabled(COMMAND_NAME);
}

function select(query) {
  let target = output.querySelector(query);

  output.focus();
  output.selectedItem = target;

  return target;
}

function testWithoutNetActivity() {
  HUD.jsterm.clearOutput();
  output = HUD.outputNode;
  content.wrappedJSObject.console.log("bug 638949");

  // Test that "Copy Link Location" command is disabled if there
  // were no network-related messages.
  waitForSuccess({
    name: "no net activity in console",

    validatorFn: function () {
      return output.textContent.indexOf("bug 638949") > -1;
    },

    successFn: function () {
      select(".webconsole-msg-log");
      goUpdateCommand(COMMAND_NAME);
      ok(!isEnabled(), COMMAND_NAME + "is disabled");
      executeSoon(testMenuWithoutNetActivity);
    }
  });
}

function testMenuWithoutNetActivity() {
  // Test that "Copy Link Location" menu item is hidden if there
  // were no network-related messages.
  let target = select(".webconsole-msg-log");

  function next() {
    menu.hidePopup();
    executeSoon(testWithNetActivity);
  }

  waitForOpenContextMenu(menu, {
    target: target,

    successFn: function () {
      let isHidden = menu.querySelector(CONTEXT_MENU_ID).hidden;
      ok(isHidden, CONTEXT_MENU_ID + " is hidden");
      next();
    },

    failureFn: next
  });
}

function testWithNetActivity() {
  HUD.jsterm.clearOutput();
  content.location.reload(); // Reloading will produce network logging

  // Test that "Copy Link Location" command is enabled and works
  // as expected if there were any network-related messages.
  //
  // This command should copy only the URL without timestamp and other
  // stuff.
  waitForSuccess({
    name: "net activity in console",

    validatorFn: function () {
      let item = select(".webconsole-msg-network");
      return item && item.url;
    },

    successFn: function () {
      output.focus();
      goUpdateCommand(COMMAND_NAME);
      ok(isEnabled(), COMMAND_NAME + " is enabled");

      waitForClipboard(output.selectedItem.url, function clipboardSetup() {
        goDoCommand(COMMAND_NAME);
      }, testMenuWithNetActivity, testMenuWithNetActivity);
    },

    failureFn: testMenuWithNetActivity
  });
}

function testMenuWithNetActivity() {
  // Test that "Copy Link Location" menu item is visible if there
  // were any network-related messages.
  let target = select(".webconsole-msg-network");

  function next() {
    menu.hidePopup();
    executeSoon(finalize);
  }

  waitForOpenContextMenu(menu, {
    target: target,

    successFn: function () {
      let isVisible = !menu.querySelector(CONTEXT_MENU_ID).hidden;
      ok(isVisible, CONTEXT_MENU_ID + " is visible");
      next();
    },

    failureFn: next
  });
}

function finalize() {
  HUD = null;
  output = null;
  menu = null;
  finishTest();
}
