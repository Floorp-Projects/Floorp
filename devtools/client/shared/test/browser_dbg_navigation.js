/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check tab attach/navigation.
 */

const TAB1_URL = EXAMPLE_URL + "doc_empty-tab-01.html";
const TAB2_FILE = "doc_empty-tab-02.html";
const TAB2_URL = EXAMPLE_URL + TAB2_FILE;

add_task(async () => {
  const tab = await addTab(TAB1_URL);
  const target = await createAndAttachTargetForTab(tab);

  await testNavigate(target);
  await testTargetDestroyed(target);
});

function testNavigate(target) {
  return new Promise(resolve => {
    let gotStartPacket = false;
    target.on("tabNavigated", function onTabNavigated(packet) {
      is(
        packet.url.split("/").pop(),
        TAB2_FILE,
        "Got a tab navigation notification."
      );

      info(JSON.stringify(packet));

      if (packet.state == "start") {
        ok(true, "Tab started to navigate.");
        gotStartPacket = true;
      } else if (gotStartPacket) {
        ok(true, "Tab finished navigating.");
        target.off("tabNavigated", onTabNavigated);
        resolve();
      }
    });

    BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TAB2_URL);
  });
}

async function testTargetDestroyed(target) {
  // We can't listen for tabDetached at it is received by Target first
  // and target is destroyed
  const onDetached = target.once("target-destroyed");

  removeTab(gBrowser.selectedTab);

  await onDetached;
  ok(true, "Got a tab detach notification.");
}
