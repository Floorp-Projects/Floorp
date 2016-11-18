/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that Security details tab contains the expected data.
 */

add_task(function* () {
  let { tab, monitor } = yield initNetMonitor(CUSTOM_GET_URL);
  let { $, EVENTS, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu, NetworkDetails } = NetMonitorView;
  RequestsMenu.lazyUpdate = false;

  info("Performing a secure request.");
  const REQUESTS_URL = "https://example.com" + CORS_SJS_PATH;
  let wait = waitForNetworkEvents(monitor, 1);
  yield ContentTask.spawn(tab.linkedBrowser, REQUESTS_URL, function* (url) {
    content.wrappedJSObject.performRequests(1, url);
  });
  yield wait;

  info("Selecting the request.");
  RequestsMenu.selectedIndex = 0;

  info("Waiting for details pane to be updated.");
  yield monitor.panelWin.once(EVENTS.TAB_UPDATED);

  info("Selecting security tab.");
  NetworkDetails.widget.selectedIndex = 5;

  info("Waiting for security tab to be updated.");
  yield monitor.panelWin.once(EVENTS.TAB_UPDATED);

  let errorbox = $("#security-error");
  let infobox = $("#security-information");

  is(errorbox.hidden, true, "Error box is hidden.");
  is(infobox.hidden, false, "Information box visible.");

  // Connection

  // The protocol will be TLS but the exact version depends on which protocol
  // the test server example.com supports.
  let protocol = $("#security-protocol-version-value").value;
  ok(protocol.startsWith("TLS"), "The protocol " + protocol + " seems valid.");

  // The cipher suite used by the test server example.com might change at any
  // moment but all of them should start with "TLS_".
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xhtml
  let suite = $("#security-ciphersuite-value").value;
  ok(suite.startsWith("TLS_"), "The suite " + suite + " seems valid.");

  // Host
  checkLabel("#security-info-host-header", "Host example.com:");
  checkLabel("#security-http-strict-transport-security-value", "Disabled");
  checkLabel("#security-public-key-pinning-value", "Disabled");

  // Cert
  checkLabel("#security-cert-subject-cn", "example.com");
  checkLabel("#security-cert-subject-o", "<Not Available>");
  checkLabel("#security-cert-subject-ou", "<Not Available>");

  checkLabel("#security-cert-issuer-cn", "Temporary Certificate Authority");
  checkLabel("#security-cert-issuer-o", "Mozilla Testing");
  checkLabel("#security-cert-issuer-ou", "<Not Available>");

  // Locale sensitive and varies between timezones. Cant't compare equality or
  // the test fails depending on which part of the world the test is executed.
  checkLabelNotEmpty("#security-cert-validity-begins");
  checkLabelNotEmpty("#security-cert-validity-expires");

  checkLabelNotEmpty("#security-cert-sha1-fingerprint");
  checkLabelNotEmpty("#security-cert-sha256-fingerprint");
  yield teardown(monitor);

  /**
   * A helper that compares value attribute of a label with given selector to the
   * expected value.
   */
  function checkLabel(selector, expected) {
    info("Checking label " + selector);

    let element = $(selector);

    ok(element, "Selector matched an element.");
    is(element.value, expected, "Label has the expected value.");
  }

  /**
   * A helper that checks the label with given selector is not an empty string.
   */
  function checkLabelNotEmpty(selector) {
    info("Checking that label " + selector + " is non-empty.");

    let element = $(selector);

    ok(element, "Selector matched an element.");
    isnot(element.value, "", "Label was not empty.");
  }
});
