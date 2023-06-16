/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const EMPTY_URL = "/browser/dom/workers/test/empty.html";
const WORKER_URL = "/browser/dom/workers/test/empty_worker.js";

add_task(async function () {
  let tab = BrowserTestUtils.addTab(
    gBrowser,
    "http://mochi.test:8888" + EMPTY_URL
  );
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  await SpecialPowers.spawn(
    browser,
    ["http://example.org" + WORKER_URL],
    function (spec) {
      return new content.Promise((resolve, reject) => {
        let w = new content.window.Worker(spec);
        w.onerror = _ => {
          resolve();
        };
        w.onmessage = _ => {
          reject();
        };
      });
    }
  );
  ok(
    true,
    "The worker is not loaded when the script is from different origin."
  );

  BrowserTestUtils.removeTab(tab);
});

add_task(async function () {
  let tab = BrowserTestUtils.addTab(
    gBrowser,
    "https://example.org" + EMPTY_URL
  );
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  await SpecialPowers.spawn(
    browser,
    ["http://example.org" + WORKER_URL],
    function (spec) {
      return new content.Promise((resolve, reject) => {
        let w = new content.window.Worker(spec);
        w.onerror = _ => {
          resolve();
        };
        w.onmessage = _ => {
          reject();
        };
      });
    }
  );
  ok(
    true,
    "The worker is not loaded when the script is from different origin."
  );

  BrowserTestUtils.removeTab(tab);
});
