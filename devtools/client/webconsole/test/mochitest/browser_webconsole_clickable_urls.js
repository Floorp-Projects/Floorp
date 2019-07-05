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
  const currentTab = gBrowser.selectedTab;

  const firstURL = "http://example.com/";
  const secondURL = "http://example.com/?id=secondURL";
  ContentTask.spawn(gBrowser.selectedBrowser, [firstURL, secondURL], urls => {
    content.wrappedJSObject.console.log("Visit ", urls[0], " and ", urls[1]);
  });

  const node = await waitFor(() => findMessage(hud, firstURL));
  const [urlEl1, urlEl2] = Array.from(node.querySelectorAll("a.url"));

  let onTabLoaded = BrowserTestUtils.waitForNewTab(gBrowser, firstURL, true);

  info("Clicking on the first link");
  urlEl1.click();

  let newTab = await onTabLoaded;
  // We only need to check that newTab is truthy since
  // BrowserTestUtils.waitForNewTab checks the URL.
  ok(newTab, "The expected tab was opened.");

  info("Select the first tab again");
  gBrowser.selectedTab = currentTab;

  info("Ctrl/Cmd + Click on the second link");
  onTabLoaded = BrowserTestUtils.waitForNewTab(gBrowser, secondURL, true);

  const isMacOS = Services.appinfo.OS === "Darwin";
  EventUtils.sendMouseEvent(
    {
      type: "click",
      [isMacOS ? "metaKey" : "ctrlKey"]: true,
    },
    urlEl2,
    hud.ui.window
  );

  newTab = await onTabLoaded;

  ok(newTab, "The expected tab was opened.");
  is(
    newTab._tPos,
    currentTab._tPos + 1,
    "The new tab was opened in the position to the right of the current tab"
  );
  is(gBrowser.selectedTab, currentTab, "The tab was opened in the background");

  info(
    "Test that Ctrl/Cmd + Click on a link in an array doesn't open the sidebar"
  );
  const onMessage = waitForMessage(hud, "Visit");
  ContentTask.spawn(gBrowser.selectedBrowser, firstURL, url => {
    content.wrappedJSObject.console.log([`Visit ${url}`]);
  });
  const message = await onMessage;
  const urlEl3 = message.node.querySelector("a.url");

  onTabLoaded = BrowserTestUtils.waitForNewTab(gBrowser, firstURL, true);

  EventUtils.sendMouseEvent(
    {
      type: "click",
      [isMacOS ? "metaKey" : "ctrlKey"]: true,
    },
    urlEl3,
    hud.ui.window
  );
  await onTabLoaded;

  info("Log a message and wait for it to appear so we know the UI was updated");
  const onSmokeMessage = waitForMessage(hud, "smoke");
  ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.wrappedJSObject.console.log("smoke");
  });
  await onSmokeMessage;
  ok(!hud.ui.document.querySelector(".sidebar"), "Sidebar wasn't closed");
});
