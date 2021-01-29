/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that enabling Service Workers testing option enables the
// mServiceWorkersTestingEnabled attribute added to nsPIDOMWindow.

const TEST_URI =
  URL_ROOT + "browser_toolbox_options_enable_serviceworkers_testing.html";
const ELEMENT_ID = "devtools-enable-serviceWorkersTesting";

add_task(async function() {
  await pushPref("dom.serviceWorkers.exemptFromPerDomainMax", true);
  await pushPref("dom.serviceWorkers.enabled", true);
  await pushPref("dom.serviceWorkers.testing.enabled", false);
  // Force the test to start without service worker testing enabled
  await pushPref("devtools.serviceWorkers.testing.enabled", false);

  const tab = await addTab(TEST_URI);
  const toolbox = await openToolboxForTab(tab, "options");

  let data = await register();
  is(data.success, false, "Register should fail with security error");

  const panel = toolbox.getCurrentPanel();
  const cbx = panel.panelDoc.getElementById(ELEMENT_ID);
  is(cbx.checked, false, "The checkbox shouldn't be checked");

  info(`Checking checkbox to enable service workers testing`);
  cbx.scrollIntoView();
  cbx.click();

  await refreshTab();

  data = await register();
  is(data.success, true, "Register should success");

  await unregister();
  data = await registerAndUnregisterInFrame();
  is(data.success, true, "Register should success");

  info("Workers should be turned back off when we closes the toolbox");
  await toolbox.destroy();

  await refreshTab();
  data = await register();
  is(data.success, false, "Register should fail with security error");
});

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
