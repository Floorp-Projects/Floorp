/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {ELLIPSIS} = require("devtools/shared/l10n");

add_task(async function() {
  info("Test short URL linkification JSON started");

  const url = "http://example.com/";
  const tab = await addJsonViewTab("data:application/json," + JSON.stringify([url]));
  await testLinkNavigation({ browser: tab.linkedBrowser, url });

  info("Switch back to the JSONViewer");
  await BrowserTestUtils.switchTab(gBrowser, tab);

  await testLinkNavigation({ browser: tab.linkedBrowser, url, clickLabel: true });
});

add_task(async function() {
  info("Test long URL linkification JSON started");

  const url = "http://example.com/" + "a".repeat(100);
  const tab = await addJsonViewTab("data:application/json," + JSON.stringify([url]));

  await testLinkNavigation({ browser: tab.linkedBrowser, url });

  info("Switch back to the JSONViewer");
  await BrowserTestUtils.switchTab(gBrowser, tab);

  await testLinkNavigation({
    browser: tab.linkedBrowser,
    url,
    urlText: url.slice(0, 24) + ELLIPSIS + url.slice(-24),
    clickLabel: true,
  });
});

/**
 * Assert that the expected link is displayed and that clicking on it navigates to the
 * expected url.
 *
 * @param {Object} option object containing:
 *        - browser (mandatory): the browser the tab will be opened in.
 *        - url (mandatory): The url we should navigate to.
 *        - urlText: The expected displayed text of the url.
 *                   Falls back to `url` if not filled
 *        - clickLabel: Should we click the label before doing assertions.
 */
async function testLinkNavigation({
  browser,
  url,
  urlText,
  clickLabel = false
}) {
  const onTabLoaded = BrowserTestUtils.waitForNewTab(gBrowser, url);

  ContentTask.spawn(browser, [urlText || url, clickLabel], (args) => {
    const [expectedURL, shouldClickLabel] = args;
    const {document} = content;

    if (shouldClickLabel === true) {
      document.querySelector(".jsonPanelBox .treeTable .treeLabel").click();
    }

    const link = document.querySelector(".jsonPanelBox .treeTable .treeValueCell a");
    is(link.textContent, expectedURL, "The expected URL is displayed.");
    link.click();
  });

  const newTab = await onTabLoaded;
  // We only need to check that newTab is truthy since
  // BrowserTestUtils.waitForNewTab checks the URL.
  ok(newTab, "The expected tab was opened.");
}
