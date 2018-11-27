/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
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
  const target = await TargetFactory.forTab(tab);
  await target.attach();
  const targetFront = target.activeTab;

  await testNavigate(targetFront);
  await testDetach(target);
});

function testNavigate(targetFront) {
  const outstanding = [promise.defer(), promise.defer()];

  targetFront.on("tabNavigated", function onTabNavigated(packet) {
    is(packet.url.split("/").pop(), TAB2_FILE,
      "Got a tab navigation notification.");

    info(JSON.stringify(packet));

    if (packet.state == "start") {
      ok(true, "Tab started to navigate.");
      outstanding[0].resolve();
    } else {
      ok(true, "Tab finished navigating.");
      targetFront.off("tabNavigated", onTabNavigated);
      outstanding[1].resolve();
    }
  });

  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TAB2_URL);
  return promise.all(outstanding.map(e => e.promise));
}

async function testDetach(target) {
  // We can't listen for tabDetached at it is received by Target first
  // and target is destroyed
  const onDetached = target.once("close");

  removeTab(gBrowser.selectedTab);

  await onDetached;
  ok(true, "Got a tab detach notification.");
}
