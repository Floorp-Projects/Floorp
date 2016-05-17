/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that enabling Service Workers testing option enables the
// mServiceWorkersTestingEnabled attribute added to nsPIDOMWindow.

const COMMON_FRAME_SCRIPT_URL =
  "chrome://devtools/content/shared/frame-script-utils.js";
const ROOT_TEST_DIR =
  getRootDirectory(gTestPath);
const FRAME_SCRIPT_URL =
  ROOT_TEST_DIR +
  "browser_toolbox_options_enable_serviceworkers_testing_frame_script.js";
const TEST_URI = URL_ROOT +
                 "browser_toolbox_options_enable_serviceworkers_testing.html";

const ELEMENT_ID = "devtools-enable-serviceWorkersTesting";

var toolbox;

function test() {
  // Note: Pref dom.serviceWorkers.testing.enabled is false since we are testing
  // the same capabilities are enabled with the devtool pref.
  SpecialPowers.pushPrefEnv({"set": [
    ["dom.serviceWorkers.exemptFromPerDomainMax", true],
    ["dom.serviceWorkers.enabled", true],
    ["dom.serviceWorkers.testing.enabled", false]
  ]}, init);
}

function init() {
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let linkedBrowser = tab.linkedBrowser;

  linkedBrowser.messageManager.loadFrameScript(COMMON_FRAME_SCRIPT_URL, false);
  linkedBrowser.messageManager.loadFrameScript(FRAME_SCRIPT_URL, false);

  gBrowser.selectedBrowser.addEventListener("load", function onLoad(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, onLoad, true);
    gDevTools.showToolbox(target).then(testSelectTool);
  }, true);

  content.location = TEST_URI;
}

function testSelectTool(aToolbox) {
  toolbox = aToolbox;
  toolbox.once("options-selected", start);
  toolbox.selectTool("options");
}

function register() {
  return executeInContent("devtools:sw-test:register");
}

function unregister(swr) {
  return executeInContent("devtools:sw-test:unregister");
}

function registerAndUnregisterInFrame() {
  return executeInContent("devtools:sw-test:iframe:register-and-unregister");
}

function testRegisterFails(data) {
  is(data.success, false, "Register should fail with security error");
  return promise.resolve();
}

function toggleServiceWorkersTestingCheckbox() {
  let panel = toolbox.getCurrentPanel();
  let cbx = panel.panelDoc.getElementById(ELEMENT_ID);

  cbx.scrollIntoView();

  if (cbx.checked) {
    info("Clearing checkbox to disable service workers testing");
  } else {
    info("Checking checkbox to enable service workers testing");
  }

  cbx.click();

  return promise.resolve();
}

function reload() {
  let deferred = promise.defer();

  gBrowser.selectedBrowser.addEventListener("load", function onLoad(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, onLoad, true);
    deferred.resolve();
  }, true);

  executeInContent("devtools:test:reload", {}, {}, false);
  return deferred.promise;
}

function testRegisterSuccesses(data) {
  is(data.success, true, "Register should success");
  return promise.resolve();
}

function start() {
  register()
    .then(testRegisterFails)
    .then(toggleServiceWorkersTestingCheckbox)
    .then(reload)
    .then(register)
    .then(testRegisterSuccesses)
    .then(unregister)
    .then(registerAndUnregisterInFrame)
    .then(testRegisterSuccesses)
    // Workers should be turned back off when we closes the toolbox
    .then(toolbox.destroy.bind(toolbox))
    .then(reload)
    .then(register)
    .then(testRegisterFails)
    .catch(function (e) {
      ok(false, "Some test failed with error " + e);
    }).then(finishUp);
}

function finishUp() {
  gBrowser.removeCurrentTab();
  toolbox = null;
  finish();
}
