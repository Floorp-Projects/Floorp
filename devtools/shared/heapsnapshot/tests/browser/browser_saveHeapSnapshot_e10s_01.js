/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 1201597 - Test to verify that we can take a heap snapshot in an e10s child process.
 */

"use strict";

add_task(async function () {
  // Create a minimal browser
  const browser = document.createXULElement("browser");
  browser.setAttribute("type", "content");
  document.body.appendChild(browser);
  await BrowserTestUtils.browserLoaded(browser);

  info("Save heap snapshot");
  const result = await SpecialPowers.spawn(browser, [], () => {
    try {
      ChromeUtils.saveHeapSnapshot({ runtime: true });
    } catch (err) {
      return err.toString();
    }

    return "";
  });
  is(result, "", "result of saveHeapSnapshot");

  browser.remove();
});
