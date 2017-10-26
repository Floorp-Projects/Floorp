/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function runTests() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:about");

  registerCleanupFunction(function() {
    gBrowser.removeTab(tab);
  });

  let browser = tab.linkedBrowser;

  browser.loadURI("about:accounts");
  let href = await BrowserTestUtils.browserLoaded(browser);
  is(href, "about:accounts", "Check about:accounts loaded");

  // Using a dummy onunload listener to disable the bfcache as that can prevent
  // the test browser load detection mechanism from working.
  browser.loadURI("data:text/html,<body%20onunload=''><iframe></iframe></body>");
  href = await BrowserTestUtils.browserLoaded(browser);
  is(href, "data:text/html,<body%20onunload=''><iframe></iframe></body>",
    "Check data URL loaded");

  browser.goBack();
  href = await BrowserTestUtils.browserLoaded(browser);
  is(href, "about:accounts", "Check we've gone back to about:accounts");

  browser.goForward();
  href = await BrowserTestUtils.browserLoaded(browser);
  is(href, "data:text/html,<body%20onunload=''><iframe></iframe></body>",
     "Check we've gone forward to data URL");
});
