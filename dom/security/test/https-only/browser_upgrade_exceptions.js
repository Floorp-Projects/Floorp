// Bug 1625448 - HTTPS Only Mode - Exceptions for loopback and local IP addresses
// https://bugzilla.mozilla.org/show_bug.cgi?id=1631384
// This test ensures that various configurable upgrade exceptions work
"use strict";

add_task(async function() {
  requestLongerTimeout(2);

  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_only_mode", true]],
  });

  // Loopback test
  await runTest(
    "Loopback IP addresses should always be exempt from upgrades (127.0.0.1)",
    "http://localhost",
    "http://"
  );
  await runTest(
    "Loopback IP addresses should always be exempt from upgrades (127.0.0.1)",
    "http://127.0.0.1",
    "http://"
  );
  // Default local-IP and onion tests
  await runTest(
    "Local IP addresses should be exempt from upgrades by default",
    "http://10.0.250.250",
    "http://"
  );
  await runTest(
    "Hosts ending with .onion should be be exempt from HTTPS-Only upgrades by default",
    "http://grocery.shopping.for.one.onion",
    "http://"
  );

  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.security.https_only_mode.upgrade_local", true],
      ["dom.security.https_only_mode.upgrade_onion", true],
    ],
  });

  // Local-IP and onion tests with upgrade enabled
  await runTest(
    "Local IP addresses should get upgraded when 'dom.security.https_only_mode.upgrade_local' is set to true",
    "http://10.0.250.250",
    "https://"
  );
  await runTest(
    "Hosts ending with .onion should get upgraded when 'dom.security.https_only_mode.upgrade_onion' is set to true",
    "http://grocery.shopping.for.one.onion",
    "https://"
  );
  // Local-IP request with HTTPS_ONLY_EXEMPT flag
  await runTest(
    "The HTTPS_ONLY_EXEMPT flag should overrule upgrade-prefs",
    "http://10.0.250.250",
    "http://",
    true
  );
});

async function runTest(desc, url, startsWith, exempt = false) {
  const responseURL = await new Promise(resolve => {
    let xhr = new XMLHttpRequest();
    xhr.timeout = 1200;
    xhr.open("GET", url);
    if (exempt) {
      xhr.channel.loadInfo.httpsOnlyStatus |= Ci.nsILoadInfo.HTTPS_ONLY_EXEMPT;
    }
    xhr.onreadystatechange = () => {
      // We don't care about the result and it's possible that
      // the requests might even succeed in some testing environments
      if (
        xhr.readyState !== XMLHttpRequest.OPENED ||
        xhr.readyState !== XMLHttpRequest.UNSENT
      ) {
        // Let's make sure this function doesn't get caled anymore
        xhr.onreadystatechange = undefined;
        resolve(xhr.responseURL);
      }
    };
    xhr.send();
  });
  ok(responseURL.startsWith(startsWith), desc);
}
