/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Test that warning indicators are shown when appropriate.
 */

const TEST_CASES = [
  {
    desc: "no warnings",
    uri: "https://example.com" + CORS_SJS_PATH,
    warnCipher: false,
    warnSSLv3: false,
  },
  {
    desc: "sslv3 warning",
    uri: "https://ssl3.example.com" + CORS_SJS_PATH,
    warnCipher: false,
    warnSSLv3: true,
  },
  {
    desc: "cipher warning",
    uri: "https://rc4.example.com" + CORS_SJS_PATH,
    warnCipher: true,
    warnSSLv3: false,
  },
  {
    desc: "cipher and sslv3 warning",
    uri: "https://ssl3rc4.example.com" + CORS_SJS_PATH,
    warnCipher: true,
    warnSSLv3: true,
  },
];

add_task(function* () {
  let [tab, debuggee, monitor] = yield initNetMonitor(CUSTOM_GET_URL);
  let { $, EVENTS, NetMonitorView } = monitor.panelWin;
  let { RequestsMenu, NetworkDetails } = NetMonitorView;
  RequestsMenu.lazyUpdate = false;

  info("Enabling SSLv3 and RC4 for the test.");
  yield new promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": [
      ["security.tls.version.min", 0],
      ["security.tls.insecure_fallback_hosts", "rc4.example.com,ssl3rc4.example.com"]
    ]}, resolve);
  });

  let cipher = $("#security-warning-cipher");
  let sslv3 = $("#security-warning-sslv3");

  for (let test of TEST_CASES) {
    info("Testing site with " + test.desc);

    info("Performing request to " + test.uri);
    debuggee.performRequests(1, test.uri);
    yield waitForNetworkEvents(monitor, 1);

    info("Selecting the request.");
    RequestsMenu.selectedIndex = 0;

    info("Waiting for details pane to be updated.");
    yield monitor.panelWin.once(EVENTS.TAB_UPDATED);

    if (NetworkDetails.widget.selectedIndex !== 5) {
      info("Selecting security tab.");
      NetworkDetails.widget.selectedIndex = 5;

      info("Waiting for details pane to be updated.");
      yield monitor.panelWin.once(EVENTS.TAB_UPDATED);
    }

    is(cipher.hidden, !test.warnCipher, "Cipher suite warning is hidden.");
    is(sslv3.hidden, !test.warnSSLv3, "SSLv3 warning is hidden.");

    RequestsMenu.clear();

  }

  yield teardown(monitor);

});
