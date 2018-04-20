// Description of the test:
//   Ensure that FTP subresource loads trigger a warning in the webconsole.
'use strict';

function scopedCuImport(path) {
  const scope = {};
  ChromeUtils.import(path, scope);
  return scope;
}
const {loader, require} = scopedCuImport("resource://devtools/shared/Loader.jsm");
const {TargetFactory} = require("devtools/client/framework/target");
const {Utils: WebConsoleUtils} =
  require("devtools/client/webconsole/utils");
let { gDevTools } = require("devtools/client/framework/devtools");
let promise = require("promise");

/**
 * Open the toolbox in a given tab.
 * @param {XULNode} tab The tab the toolbox should be opened in.
 * @param {String} toolId Optional. The ID of the tool to be selected.
 * @param {String} hostType Optional. The type of toolbox host to be used.
 * @return {Promise} Resolves with the toolbox, when it has been opened.
 */
var openToolboxForTab = async function(tab, toolId, hostType) {
  info("Opening the toolbox");

  let toolbox;
  let target = TargetFactory.forTab(tab);
  await target.makeRemote();

  // Check if the toolbox is already loaded.
  toolbox = gDevTools.getToolbox(target);
  if (toolbox) {
    if (!toolId || (toolId && toolbox.getPanel(toolId))) {
      info("Toolbox is already opened");
      return toolbox;
    }
  }

  // If not, load it now.
  toolbox = await gDevTools.showToolbox(target, toolId, hostType);

  // Make sure that the toolbox frame is focused.
  await new Promise(resolve => waitForFocus(resolve, toolbox.win));

  info("Toolbox opened and focused");

  return toolbox;
};


function console_observer(subject, topic, data) {
  var message = subject.wrappedJSObject.arguments[0];
  ok(false, message);
};

var webconsole = null;
// These files don't actually exist, we are just looking for messages
// that indicate that loading those files would have been blocked.
var seen_files = ["a.html", "b.html", "c.html", "d.png"];

function on_new_message(new_messages) {
  for (let message of new_messages) {
    let elem = message.node;
    let text = elem.textContent;

    if (text.includes("Loading FTP subresource within http(s) page not allowed")) {
      // Remove the file in the message from the list.
      seen_files = seen_files.filter(file => {
        return !text.includes(file);
      });
    }
  }
}

async function do_cleanup() {
  if (webconsole) {
    webconsole.ui.off("new-messages", on_new_message);
  }
}

const kTestPath = getRootDirectory(gTestPath)
                  .replace("chrome://mochitests/content", "http://mochi.test:8888")
const kTestURI = kTestPath + "file_FTP_console_warning.html";

add_task(async function() {
  // A longer timeout is necessary for this test than the plain mochitests
  // due to opening a new tab with the web console.
  requestLongerTimeout(4);
  registerCleanupFunction(do_cleanup);

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

  let toolbox = await openToolboxForTab(tab, "webconsole");
  ok(toolbox, "Got toolbox");
  let hud = toolbox.getCurrentPanel().hud;
  ok(hud, "Got hud");

  if (!webconsole) {
    registerCleanupFunction(do_cleanup);
    hud.ui.on("new-messages", on_new_message);
    webconsole = hud;
  }

  await BrowserTestUtils.loadURI(gBrowser.selectedBrowser, kTestURI);

  await BrowserTestUtils.waitForCondition(() => seen_files.length === 0);

  is(seen_files.length, 0, "All FTP subresources should be blocked");

  BrowserTestUtils.removeTab(tab);
});
