/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function runTests() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.navigation.requireUserInteraction", false]],
  });
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:about"
  );

  registerCleanupFunction(function () {
    gBrowser.removeTab(tab);
  });

  let browser = tab.linkedBrowser;

  let loaded = BrowserTestUtils.browserLoaded(browser);
  BrowserTestUtils.startLoadingURIString(browser, "about:config");
  let href = await loaded;
  is(href, "about:config", "Check about:config loaded");

  // Using a dummy onunload listener to disable the bfcache as that can prevent
  // the test browser load detection mechanism from working.
  loaded = BrowserTestUtils.browserLoaded(browser);
  BrowserTestUtils.startLoadingURIString(
    browser,
    "data:text/html,<body%20onunload=''><iframe></iframe></body>"
  );
  href = await loaded;
  is(
    href,
    "data:text/html,<body%20onunload=''><iframe></iframe></body>",
    "Check data URL loaded"
  );

  loaded = BrowserTestUtils.browserLoaded(browser);
  browser.goBack();
  href = await loaded;
  is(href, "about:config", "Check we've gone back to about:config");

  loaded = BrowserTestUtils.browserLoaded(browser);
  browser.goForward();
  href = await loaded;
  is(
    href,
    "data:text/html,<body%20onunload=''><iframe></iframe></body>",
    "Check we've gone forward to data URL"
  );
});
