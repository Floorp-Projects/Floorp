/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Testing that the inspector doesn't go blank when navigating to a page that
// deletes an iframe while loading.

const TEST_URL = URL_ROOT + "doc_inspector_remove-iframe-during-load.html";

add_task(async function () {
  const { inspector, tab } = await openInspectorForURL("about:blank");
  await selectNode("body", inspector);

  // Before we start navigating, attach a listener on the reloaded event.
  const onInspectorReloaded = inspector.once("reloaded");

  // Note: here we don't want to use the `navigateTo` helper from shared-head.js
  // because we want to modify the page as early as possible after the
  // navigation, ideally before the inspector has fully initialized.
  // See next comments.
  const browser = tab.linkedBrowser;
  const onBrowserLoaded = BrowserTestUtils.browserLoaded(browser);
  BrowserTestUtils.startLoadingURIString(browser, TEST_URL);
  await onBrowserLoaded;

  // We do not want to wait for the inspector to be fully ready before testing
  // so we load TEST_URL and just wait for the content window to be done loading
  await SpecialPowers.spawn(browser, [], async function () {
    await content.wrappedJSObject.readyPromise;
  });

  // The content doc contains a script that creates iframes and deletes them
  // immediately after. It does this before the load event, after
  // DOMContentLoaded and after load. This is what used to make the inspector go
  // blank when navigating to that page.
  // At this stage, there should be no iframes in the page anymore.
  ok(
    !(await contentPageHasNode(browser, "iframe")),
    "Iframes added by the content page should have been removed"
  );

  // Create/remove an extra one now, after the load event.
  info("Creating and removing an iframe.");
  await SpecialPowers.spawn(browser, [], async function () {
    const iframe = content.document.createElement("iframe");
    content.document.body.appendChild(iframe);
    iframe.remove();
  });

  ok(
    !(await contentPageHasNode(browser, "iframe")),
    "The after-load iframe should have been removed."
  );

  // Assert that the markup-view is displayed and works
  ok(!(await contentPageHasNode(browser, "iframe")), "Iframe has been removed");

  const expectedText = await SpecialPowers.spawn(
    browser,
    [],
    async function () {
      return content.document.querySelector("#yay").textContent;
    }
  );
  is(expectedText, "load", "Load event fired.");

  info("Wait for the inspector to be properly reloaded");
  await onInspectorReloaded;

  // Smoke test to check that the inspector can still select nodes and hasn't
  // gone blank.
  await selectNode("#yay", inspector);
});

function contentPageHasNode(browser, selector) {
  return SpecialPowers.spawn(
    browser,
    [selector],
    async function (selectorChild) {
      return !!content.document.querySelector(selectorChild);
    }
  );
}
