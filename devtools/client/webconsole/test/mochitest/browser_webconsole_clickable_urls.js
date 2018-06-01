/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// When strings containing URLs are entered into the webconsole,
// ensure that the output can be clicked to open those URLs.
// This test only check that clicking on a link works as expected,
// as the output is already tested in Reps (in Github).

"use strict";

const TEST_URI = "data:text/html;charset=utf8,Clickable URLS";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  const url = "http://example.com/";
  await ContentTask.spawn(gBrowser.selectedBrowser, url, function(uri) {
    content.wrappedJSObject.console.log(uri);
  });

  const node = await waitFor(() => findMessage(hud, url));
  const link = node.querySelector("a.url");

  const onTabLoaded = BrowserTestUtils.waitForNewTab(gBrowser, url, true);

  info("Clicking on the link");
  link.click();

  const newTab = await onTabLoaded;
  // We only need to check that newTab is truthy since
  // BrowserTestUtils.waitForNewTab checks the URL.
  ok(newTab, "The expected tab was opened.");
  BrowserTestUtils.removeTab(newTab);
});
