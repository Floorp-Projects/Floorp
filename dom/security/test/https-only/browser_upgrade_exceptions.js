// Bug 1625448 - HTTPS Only Mode - Exceptions for loopback and local IP addresses
// https://bugzilla.mozilla.org/show_bug.cgi?id=1631384
// This test ensures that various configurable upgrade exceptions work
"use strict";

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_only_mode", true]],
  });

  await Promise.all([
    // Loopback test
    runTest(
      "Loopback IP addresses should always be excempt from upgrades (localhost)",
      "http://localhost",
      "http://"
    ),
    runTest(
      "Loopback IP addresses should always be excempt from upgrades (127.0.0.1)",
      "http://127.0.0.1",
      "http://"
    ),
    // Default local-IP and onion tests
    runTest(
      "Local IP addresses should be excempt from upgrades by default",
      "http://10.0.0.1",
      "http://"
    ),
    runTest(
      "Hosts ending with .onion should be be excempt from HTTPS-Only upgrades by default",
      "http://grocery.shopping.for.one.onion",
      "http://"
    ),
  ]);

  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.security.https_only_mode.upgrade_local", true],
      ["dom.security.https_only_mode.upgrade_onion", true],
    ],
  });

  await Promise.all([
    // Local-IP and onion tests with upgrade enabled
    runTest(
      "Local IP addresses should get upgraded when 'dom.security.https_only_mode.upgrade_local' is set to true",
      "http://10.0.0.1",
      "https://"
    ),
    runTest(
      "Hosts ending with .onion should get upgraded when 'dom.security.https_only_mode.upgrade_onion' is set to true",
      "http://grocery.shopping.for.one.onion",
      "https://"
    ),
    // Local-IP request with HTTPS_ONLY_EXEMPT flag
    runTest(
      "The HTTPS_ONLY_EXEMPT flag should overrule upgrade-prefs",
      "http://10.0.0.1",
      "http://",
      true
    ),
  ]);
});

async function runTest(desc, url, startsWith, exempt = false) {
  const responseURL = await new Promise(resolve => {
    let xhr = new XMLHttpRequest();
    xhr.open("GET", url);
    if (exempt) {
      xhr.channel.loadInfo.httpsOnlyStatus |= Ci.nsILoadInfo.HTTPS_ONLY_EXEMPT;
    }
    xhr.onerror = () => {
      resolve(xhr.responseURL);
    };
    xhr.send();
  });
  ok(responseURL.startsWith(startsWith), desc);
}
