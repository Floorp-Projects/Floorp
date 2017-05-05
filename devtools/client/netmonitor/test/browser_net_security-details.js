/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that Security details tab contains the expected data.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CUSTOM_GET_URL);
  let { document, store, windowRequire } = monitor.panelWin;
  let Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  info("Performing a secure request.");
  const REQUESTS_URL = "https://example.com" + CORS_SJS_PATH;
  let wait = waitForNetworkEvents(monitor, 1);
  yield ContentTask.spawn(tab.linkedBrowser, REQUESTS_URL, function* (url) {
    content.wrappedJSObject.performRequests(1, url);
  });
  yield wait;

  wait = waitForDOM(document, "#security-panel");
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector(".network-details-panel-toggle"));
  EventUtils.sendMouseEvent({ type: "click" },
    document.querySelector("#security-tab"));
  yield wait;

  let tabpanel = document.querySelector("#security-panel");
  let textboxes = tabpanel.querySelectorAll(".textbox-input");

  // Connection
  // The protocol will be TLS but the exact version depends on which protocol
  // the test server example.com supports.
  let protocol = textboxes[0].value;
  ok(protocol.startsWith("TLS"), "The protocol " + protocol + " seems valid.");

  // The cipher suite used by the test server example.com might change at any
  // moment but all of them should start with "TLS_".
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xhtml
  let suite = textboxes[1].value;
  ok(suite.startsWith("TLS_"), "The suite " + suite + " seems valid.");

  // Host
  is(tabpanel.querySelectorAll(".treeLabel.objectLabel")[1].textContent,
     "Host example.com:",
     "Label has the expected value.");
  is(textboxes[2].value, "Disabled", "Label has the expected value.");
  is(textboxes[3].value, "Disabled", "Label has the expected value.");

  // Cert
  is(textboxes[4].value, "example.com", "Label has the expected value.");
  is(textboxes[5].value, "<Not Available>", "Label has the expected value.");
  is(textboxes[6].value, "<Not Available>", "Label has the expected value.");

  is(textboxes[7].value, "Temporary Certificate Authority",
     "Label has the expected value.");
  is(textboxes[8].value, "Mozilla Testing", "Label has the expected value.");
  is(textboxes[9].value, "Profile Guided Optimization", "Label has the expected value.");

  // Locale sensitive and varies between timezones. Cant't compare equality or
  // the test fails depending on which part of the world the test is executed.

  // cert validity begins
  isnot(textboxes[10].value, "", "Label was not empty.");
  // cert validity expires
  isnot(textboxes[11].value, "", "Label was not empty.");

  // cert sha1 fingerprint
  isnot(textboxes[12].value, "", "Label was not empty.");
  // cert sha256 fingerprint
  isnot(textboxes[13].value, "", "Label was not empty.");

  yield teardown(monitor);
});
