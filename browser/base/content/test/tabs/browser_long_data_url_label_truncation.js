/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Ensure that tab labels for base64 data: URLs are always truncated
 * to ensure that we don't hang trying to paint really long overflown
 * text runs.
 * This becomes a performance issue with 1mb or so long data URIs;
 * this test uses a much shorter one for simplicity's sake.
 */
add_task(async function test_ensure_truncation() {
  const MOBY = `
  <!DOCTYPE html>
  <meta charset="utf-8">
    Call me Ishmael. Some years ago—never mind how
    long precisely—having little or no money in my purse, and nothing particular
    to interest me on shore, I thought I would sail about a little and see the
    watery part of the world. It is a way I have of driving off the spleen and
    regulating the circulation. Whenever I find myself growing grim about the
    mouth; whenever it is a damp, drizzly November in my soul; whenever I find
    myself involuntarily pausing before coffin warehouses, and bringing up the
    rear of every funeral I meet; and especially whenever my hypos get such an
    upper hand of me, that it requires a strong moral principle to prevent me
    from deliberately stepping into the street, and methodically knocking
    people's hats off—then, I account it high time to get to sea as soon as I
    can. This is my substitute for pistol and ball. With a philosophical
    flourish Cato throws himself upon his sword; I quietly take to the ship.
    There is nothing surprising in this. If they but knew it, almost all men in
    their degree, some time or other, cherish very nearly the same feelings
    towards the ocean with me.`;

  let fileReader = new FileReader();
  const DATA_URL = await new Promise(resolve => {
    fileReader.addEventListener("load", e => resolve(fileReader.result));
    fileReader.readAsDataURL(new Blob([MOBY], { type: "text/html" }));
  });
  // Substring the full URL to avoid log clutter because Assert will print
  // the entire thing.
  Assert.stringContains(
    DATA_URL.substring(0, 30),
    "base64",
    "data URL needs to be base64"
  );

  let newTab;
  function tabLabelChecker() {
    Assert.lessOrEqual(
      newTab.label.length,
      501,
      "Tab label should not exceed 500 chars + ellipsis."
    );
  }
  let mutationObserver = new MutationObserver(tabLabelChecker);
  registerCleanupFunction(() => mutationObserver.disconnect());

  gBrowser.tabContainer.addEventListener(
    "TabOpen",
    event => {
      newTab = event.target;
      tabLabelChecker();
      mutationObserver.observe(newTab, {
        attributeFilter: ["label"],
      });
    },
    { once: true }
  );

  await BrowserTestUtils.withNewTab(DATA_URL, async () => {
    // Wait another longer-than-tick to ensure more mutation observer things have
    // come in.
    await new Promise(executeSoon);

    // Check one last time for good measure, for the final label:
    tabLabelChecker();
  });
});
