/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the Web Console doesn't leak when multiple tabs and windows are
// opened and then closed. See Bug 595350.

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console test for bug 595350";

add_task(async function() {
  requestLongerTimeout(3);
  // Bug 1518138: GC heuristics are broken for this test, so that the test
  // ends up running out of memory. Try to work-around the problem by GCing
  // before the test begins.
  Cu.forceShrinkingGC();

  const win1 = window;

  info("Add test tabs in first window");
  const tab1 = await addTab(TEST_URI, { window: win1 });
  const tab2 = await addTab(TEST_URI, { window: win1 });
  info("Test tabs added in first window");

  info("Open a second window");
  const win2 = OpenBrowserWindow();
  await new Promise(r => {
    win2.addEventListener("load", r, { capture: true, once: true });
  });

  info("Add test tabs in second window");
  const tab3 = await addTab(TEST_URI, { window: win2 });
  const tab4 = await addTab(TEST_URI, { window: win2 });

  info("Opening console in each test tab");
  const tabs = [tab1, tab2, tab3, tab4];
  for (const tab of tabs) {
    // Open the console in tab${i}.
    const hud = await openConsole(tab);
    const browser = hud.currentTarget.tab.linkedBrowser;
    const message = "message for tab " + tabs.indexOf(tab);

    // Log a message in the newly opened console.
    const onMessage = waitForMessage(hud, message);
    await ContentTask.spawn(browser, message, function(msg) {
      content.console.log(msg);
    });
    await onMessage;
  }

  const onConsolesDestroyed = waitForNEvents("web-console-destroyed", 4);

  info("Close the second window");
  win2.close();

  info("Close the test tabs in the first window");
  win1.gBrowser.removeTab(tab1);
  win1.gBrowser.removeTab(tab2);

  info("Wait for 4 web-console-destroyed events");
  await onConsolesDestroyed;

  ok(true, "Received web-console-destroyed for each console opened");
});

/**
 * Wait for N events helper customized to work with Services.obs.add/removeObserver.
 */
function waitForNEvents(expectedTopic, times) {
  return new Promise(resolve => {
    let count = 0;

    function onEvent(subject, topic) {
      if (topic !== expectedTopic) {
        return;
      }

      count++;
      info(`Received ${expectedTopic} ${count} time(s).`);
      if (count == times) {
        resolve();
      }
    }

    registerCleanupFunction(() => {
      Services.obs.removeObserver(onEvent, expectedTopic);
    });
    Services.obs.addObserver(onEvent, expectedTopic);
  });
}
