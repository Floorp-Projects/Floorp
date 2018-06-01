/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that Security details tab contains the expected data.
 */

add_task(async function() {
  await pushPref("security.pki.certificate_transparency.mode", 1);

  const { tab, monitor } = await initNetMonitor(CUSTOM_GET_URL);
  const { document, store, windowRequire } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  info("Performing a secure request.");
  const REQUESTS_URL = "https://example.com" + CORS_SJS_PATH;
  const wait = waitForNetworkEvents(monitor, 1);
  await ContentTask.spawn(tab.linkedBrowser, REQUESTS_URL, async function(url) {
    content.wrappedJSObject.performRequests(1, url);
  });
  await wait;

  store.dispatch(Actions.toggleNetworkDetails());
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#security-tab"));
  await waitUntil(() => document.querySelector(
    "#security-panel .security-info-value"));

  const tabpanel = document.querySelector("#security-panel");
  const textboxes = tabpanel.querySelectorAll(".textbox-input");

  // Connection
  // The protocol will be TLS but the exact version depends on which protocol
  // the test server example.com supports.
  const protocol = textboxes[0].value;
  ok(protocol.startsWith("TLS"), "The protocol " + protocol + " seems valid.");

  // The cipher suite used by the test server example.com might change at any
  // moment but all of them should start with "TLS_".
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xhtml
  const suite = textboxes[1].value;
  ok(suite.startsWith("TLS_"), "The suite " + suite + " seems valid.");

  // Host
  is(tabpanel.querySelectorAll(".treeLabel.objectLabel")[1].textContent,
     "Host example.com:",
     "Label has the expected value.");
  // These two values can change. So only check they're not empty.
  ok(textboxes[2].value !== "", "Label value is not empty.");
  ok(textboxes[3].value !== "", "Label value is not empty.");
  is(textboxes[4].value, "Disabled", "Label has the expected value.");
  is(textboxes[5].value, "Disabled", "Label has the expected value.");

  // Cert
  is(textboxes[6].value, "example.com", "Label has the expected value.");
  is(textboxes[7].value, "<Not Available>", "Label has the expected value.");
  is(textboxes[8].value, "<Not Available>", "Label has the expected value.");

  is(textboxes[9].value, "Temporary Certificate Authority",
     "Label has the expected value.");
  is(textboxes[10].value, "Mozilla Testing", "Label has the expected value.");
  is(textboxes[11].value, "Profile Guided Optimization", "Label has the expected value.");

  // Locale sensitive and varies between timezones. Cant't compare equality or
  // the test fails depending on which part of the world the test is executed.

  // cert validity begins
  isnot(textboxes[12].value, "", "Label was not empty.");
  // cert validity expires
  isnot(textboxes[13].value, "", "Label was not empty.");

  // cert sha1 fingerprint
  isnot(textboxes[14].value, "", "Label was not empty.");
  // cert sha256 fingerprint
  isnot(textboxes[15].value, "", "Label was not empty.");

  // Certificate transparency
  isnot(textboxes[16].value, "", "Label was not empty.");

  await teardown(monitor);
});
