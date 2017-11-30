/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// We will open a new tab if clicking on a cross domain link in pinned tab
// So, override the tests data in head.js, adding "cross: true".

_referrerTests = [
  {
    fromScheme: "http://",
    toScheme: "http://",
    cross: true,
    result: "http://test1.example.com/browser"  // full referrer
  },
  {
    fromScheme: "https://",
    toScheme: "http://",
    cross: true,
    result: ""  // no referrer when downgrade
  },
  {
    fromScheme: "https://",
    toScheme: "http://",
    policy: "origin",
    cross: true,
    result: "https://test1.example.com/"  // origin, even on downgrade
  },
  {
    fromScheme: "https://",
    toScheme: "http://",
    policy: "origin",
    rel: "noreferrer",
    cross: true,
    result: ""  // rel=noreferrer trumps meta-referrer
  },
  {
    fromScheme: "https://",
    toScheme: "https://",
    policy: "no-referrer",
    cross: true,
    result: ""  // same origin https://test1.example.com/browser
  },
  {
    fromScheme: "http://",
    toScheme: "https://",
    policy: "no-referrer",
    cross: true,
    result: ""  // cross origin http://test1.example.com
  },
];

async function startClickPinnedTabTestCase(aTestNumber) {
  info("browser_referrer_click_pinned_tab: " +
       getReferrerTestDescription(aTestNumber));
  let browser = gTestWindow.gBrowser;

  browser.pinTab(browser.selectedTab);
  someTabLoaded(gTestWindow).then(function(aNewTab) {
    checkReferrerAndStartNextTest(aTestNumber, null, aNewTab,
                                  startClickPinnedTabTestCase);
  });

  clickTheLink(gTestWindow, "testlink", {});
}

function test() {
  requestLongerTimeout(10); // slowwww shutdown on e10s
  startReferrerTest(startClickPinnedTabTestCase);
}
