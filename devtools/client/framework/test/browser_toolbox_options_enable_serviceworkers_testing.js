/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that enabling Service Workers testing option enables the
// mServiceWorkersTestingEnabled attribute added to nsPIDOMWindow.

const TEST_URI =
  URL_ROOT + "browser_toolbox_options_enable_serviceworkers_testing.html";

const ELEMENT_ID = "devtools-enable-serviceWorkersTesting";

var toolbox;

function test() {
  // Note: Pref dom.serviceWorkers.testing.enabled is false since we are testing
  // the same capabilities are enabled with the devtool pref.
  SpecialPowers.pushPrefEnv(
    {
      set: [
        ["dom.serviceWorkers.exemptFromPerDomainMax", true],
        ["dom.serviceWorkers.enabled", true],
        ["dom.serviceWorkers.testing.enabled", false],
      ],
    },
    init
  );
}

function init() {
  addTab(TEST_URI).then(async tab => {
    const target = await TargetFactory.forTab(tab);
    gDevTools.showToolbox(target).then(testSelectTool);
  });
}

function testSelectTool(aToolbox) {
  toolbox = aToolbox;
  toolbox.once("options-selected", start);
  toolbox.selectTool("options");
}

function sendMessage(name) {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [name], nameChild => {
    return new Promise(resolve => {
      const channel = new content.MessageChannel();
      content.postMessage(nameChild, "*", [channel.port2]);
      channel.port1.onmessage = function(msg) {
        resolve(msg.data);
        channel.port1.close();
      };
    });
  });
}

function register() {
  return sendMessage("devtools:sw-test:register");
}

function unregister(swr) {
  return sendMessage("devtools:sw-test:unregister");
}

function registerAndUnregisterInFrame() {
  return sendMessage("devtools:sw-test:iframe:register-and-unregister");
}

function testRegisterFails(data) {
  is(data.success, false, "Register should fail with security error");
  return promise.resolve();
}

function toggleServiceWorkersTestingCheckbox() {
  const panel = toolbox.getCurrentPanel();
  const cbx = panel.panelDoc.getElementById(ELEMENT_ID);

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
  const promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.location.reload(false);
  });
  return promise;
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
    .catch(function(e) {
      ok(false, "Some test failed with error " + e);
    })
    .then(finishUp);
}

function finishUp() {
  gBrowser.removeCurrentTab();
  toolbox = null;
  finish();
}
