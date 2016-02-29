"use strict";

const PREF = 'network.cookie.cookieBehavior';
const PAGE_URL = 'http://mochi.test:8888/browser/' +
        'browser/components/sessionstore/test/browser_1234021_page.html';
const BEHAVIOR_REJECT = 2;

add_task(function* test() {
  yield pushPrefs([PREF, BEHAVIOR_REJECT]);

  yield BrowserTestUtils.withNewTab({
    gBrowser: gBrowser,
    url: PAGE_URL
  }, function* handler(aBrowser) {
    yield TabStateFlusher.flush(aBrowser);
    ok(true, "Flush didn't time out");
  });
});
