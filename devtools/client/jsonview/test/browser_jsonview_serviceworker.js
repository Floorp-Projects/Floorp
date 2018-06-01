/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_JSON_URL = URL_ROOT + "valid_json.json";
const EMPTY_PAGE = URL_ROOT + "empty.html";
const SW = URL_ROOT + "passthrough-sw.js";

add_task(async function() {
  info("Test valid JSON with service worker started");

  await SpecialPowers.pushPrefEnv({"set": [
    ["dom.serviceWorkers.exemptFromPerDomainMax", true],
    ["dom.serviceWorkers.enabled", true],
    ["dom.serviceWorkers.testing.enabled", true],
  ]});

  const swTab = BrowserTestUtils.addTab(gBrowser, EMPTY_PAGE);
  const browser = gBrowser.getBrowserForTab(swTab);
  await BrowserTestUtils.browserLoaded(browser);
  await ContentTask.spawn(browser, { script: SW, scope: TEST_JSON_URL }, async opts => {
    const reg = await content.navigator.serviceWorker.register(opts.script,
                                                             { scope: opts.scope });
    return new content.window.Promise(resolve => {
      const worker = reg.installing;
      if (worker.state === "activated") {
        resolve();
        return;
      }
      worker.addEventListener("statechange", evt => {
        if (worker.state === "activated") {
          resolve();
        }
      });
    });
  });

  const tab = await addJsonViewTab(TEST_JSON_URL);

  ok(tab.linkedBrowser.contentPrincipal.isNullPrincipal, "Should have null principal");

  is(await countRows(), 3, "There must be three rows");

  const objectCellCount = await getElementCount(
    ".jsonPanelBox .treeTable .objectCell");
  is(objectCellCount, 1, "There must be one object cell");

  const objectCellText = await getElementText(
    ".jsonPanelBox .treeTable .objectCell");
  is(objectCellText, "", "The summary is hidden when object is expanded");

  // Clicking the value does not collapse it (so that it can be selected and copied).
  await clickJsonNode(".jsonPanelBox .treeTable .treeValueCell");
  is(await countRows(), 3, "There must still be three rows");

  // Clicking the label collapses the auto-expanded node.
  await clickJsonNode(".jsonPanelBox .treeTable .treeLabel");
  is(await countRows(), 1, "There must be one row");

  await ContentTask.spawn(browser, { script: SW, scope: TEST_JSON_URL }, async opts => {
    const reg = await content.navigator.serviceWorker.getRegistration(opts.scope);
    await reg.unregister();
  });

  BrowserTestUtils.removeTab(swTab);
});

function countRows() {
  return getElementCount(".jsonPanelBox .treeTable .treeRow");
}
