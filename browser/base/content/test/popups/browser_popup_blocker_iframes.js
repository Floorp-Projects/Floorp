/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

requestLongerTimeout(5);

const testURL = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.org"
);

const examplecomURL = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.com"
);

const w3cURL = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://w3c-test.org"
);

const examplenetURL = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.net"
);

const prefixexamplecomURL = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://prefixexample.com"
);

class TestCleanup {
  constructor() {
    this.tabs = [];
  }

  count() {
    return this.tabs.length;
  }

  static setup() {
    let cleaner = new TestCleanup();
    this.onTabOpen = event => {
      cleaner.tabs.push(event.target);
    };
    gBrowser.tabContainer.addEventListener("TabOpen", this.onTabOpen);
    return cleaner;
  }

  clean() {
    gBrowser.tabContainer.removeEventListener("TabOpen", this.onTabOpen);
    for (let tab of this.tabs) {
      gBrowser.removeTab(tab);
    }
  }
}

async function runTest(count, urls, permissions, delayedAllow) {
  let cleaner = TestCleanup.setup();

  // Enable the popup blocker.
  await SpecialPowers.pushPrefEnv({
    set: [["dom.disable_open_during_load", true]],
  });

  await SpecialPowers.pushPermissions(permissions);

  // Open the test page.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, testURL);

  let contexts = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [...urls, !!delayedAllow],
    async (url1, url2, url3, url4, delay) => {
      let iframe1 = content.document.createElement("iframe");
      let iframe2 = content.document.createElement("iframe");
      iframe1.id = "iframe1";
      iframe2.id = "iframe2";
      iframe1.src = new URL(
        `popup_blocker_frame.html?delayed=${delay}&base=${url3}`,
        url1
      );
      iframe2.src = new URL(
        `popup_blocker_frame.html?delayed=${delay}&base=${url4}`,
        url2
      );

      let promises = [
        new Promise(resolve => (iframe1.onload = resolve)),
        new Promise(resolve => (iframe2.onload = resolve)),
      ];

      content.document.body.appendChild(iframe1);
      content.document.body.appendChild(iframe2);

      await Promise.all(promises);
      return [iframe1.browsingContext, iframe2.browsingContext];
    }
  );

  if (delayedAllow) {
    await delayedAllow();
    await SpecialPowers.spawn(
      tab.linkedBrowser,
      contexts,
      async function (bc1, bc2) {
        bc1.window.postMessage("allow", "*");
        bc2.window.postMessage("allow", "*");
      }
    );
  }

  await TestUtils.waitForCondition(
    () => cleaner.count() == count,
    `waiting for ${count} tabs, got ${cleaner.count()}`
  );

  Assert.equal(cleaner.count(), count, `should have ${count} tabs`);

  await SpecialPowers.popPermissions();
  cleaner.clean();
}

add_task(async function () {
  let permission = {
    type: "popup",
    allow: true,
    context: "",
  };

  let expected = [];

  let tests = [
    [examplecomURL, w3cURL, prefixexamplecomURL, examplenetURL],
    [examplecomURL, examplecomURL, prefixexamplecomURL, examplenetURL],
    [examplecomURL, examplecomURL, prefixexamplecomURL, prefixexamplecomURL],
    [examplecomURL, w3cURL, prefixexamplecomURL, prefixexamplecomURL],
  ];

  permission.context = testURL;
  expected = [5, 5, 3, 3];
  for (let test in tests) {
    await runTest(expected[test], tests[test], [permission]);
  }

  permission.context = examplecomURL;
  expected = [3, 5, 3, 3];
  for (let test in tests) {
    await runTest(expected[test], tests[test], [permission]);
  }

  permission.context = prefixexamplecomURL;
  expected = [3, 3, 3, 3];
  for (let test in tests) {
    await runTest(expected[test], tests[test], [permission]);
  }

  async function allowPopup() {
    await SpecialPowers.pushPermissions([permission]);
  }

  permission.context = testURL;
  expected = [5, 5, 3, 3];
  for (let test in tests) {
    await runTest(expected[test], tests[test], [], allowPopup);
  }

  permission.context = examplecomURL;
  expected = [3, 5, 3, 3];
  for (let test in tests) {
    await runTest(expected[test], tests[test], [], allowPopup);
  }

  permission.context = prefixexamplecomURL;
  expected = [3, 3, 3, 3];
  for (let test in tests) {
    await runTest(expected[test], tests[test], [], allowPopup);
  }
});
