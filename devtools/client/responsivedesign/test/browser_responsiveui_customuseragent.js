/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html, Custom User Agent test";
const DEFAULT_UA = Cc["@mozilla.org/network/protocol;1?name=http"]
                    .getService(Ci.nsIHttpProtocolHandler)
                    .userAgent;
const CHROME_UA = "Mozilla/5.0 (Windows NT 6.1) AppleWebKit/537.36" +
                  " (KHTML, like Gecko) Chrome/41.0.2228.0 Safari/537.36";
add_task(function* () {
  yield addTab(TEST_URI);

  let {rdm, manager} = yield openRDM();
  yield testUserAgent(DEFAULT_UA);

  info("Setting UA to " + CHROME_UA);
  yield setUserAgent(CHROME_UA, rdm, manager);
  yield testUserAgent(CHROME_UA);

  info("Resetting UA");
  yield setUserAgent("", rdm, manager);
  yield testUserAgent(DEFAULT_UA);

  info("Setting UA to " + CHROME_UA);
  yield setUserAgent(CHROME_UA, rdm, manager);
  yield testUserAgent(CHROME_UA);

  info("Closing responsive mode");

  yield closeRDM(rdm);
  yield testUserAgent(DEFAULT_UA);
});

function* setUserAgent(ua, rdm, manager) {
  let input = rdm.userAgentInput;
  input.focus();
  input.value = ua;
  let onUAChanged = once(manager, "userAgentChanged");
  input.blur();
  yield onUAChanged;

  if (ua !== "") {
    ok(input.hasAttribute("attention"), "UA input should be highlighted");
  } else {
    ok(!input.hasAttribute("attention"), "UA input shouldn't be highlighted");
  }
}

function* testUserAgent(value) {
  let ua = yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    return content.navigator.userAgent;
  });
  is(ua, value, `UA should be set to ${value}`);
}
