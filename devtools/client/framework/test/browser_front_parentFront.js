/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the Front's parentFront attribute returns the correct parent front.

const TEST_URL = `data:text/html;charset=utf-8,<div id="test"></div>`;

add_task(async function() {
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);

  const tab = await addTab(TEST_URL);
  const target = await TargetFactory.forTab(tab);
  await target.attach();

  const inspectorFront = await target.getFront("inspector");
  const walker = inspectorFront.walker;
  const pageStyleFront = await inspectorFront.getPageStyle();
  const nodeFront = await walker.querySelector(walker.rootNode, "#test");

  is(
    inspectorFront.parentFront,
    target,
    "Got the correct parentFront from the InspectorFront."
  );
  is(
    walker.parentFront,
    inspectorFront,
    "Got the correct parentFront from the WalkerFront."
  );
  is(
    pageStyleFront.parentFront,
    inspectorFront,
    "Got the correct parentFront from the PageStyleFront."
  );
  is(
    nodeFront.parentFront,
    walker,
    "Got the correct parentFront from the NodeFront."
  );
});
