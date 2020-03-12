/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether no errors occur when reloading the browsing page.

const {
  COMPATIBILITY_UPDATE_SELECTED_NODE_FAILURE,
} = require("devtools/client/inspector/compatibility/actions/index");

add_task(async function() {
  const tab = await addTab("data:text/html;charset=utf-8,test");
  const { inspector } = await openCompatibilityView();

  let isFailureActionFired = false;
  waitForDispatch(
    inspector.store,
    COMPATIBILITY_UPDATE_SELECTED_NODE_FAILURE
  ).then(() => {
    isFailureActionFired = true;
  });

  info("Reload the browsing page");
  const onReloaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  const onUpdateAction = waitForUpdateSelectedNodeAction(inspector.store);
  gBrowser.reloadTab(tab);
  await Promise.all([onReloaded, onUpdateAction]);

  info("Check whether the failure action will be fired or not");
  ok(!isFailureActionFired, "No error occurred");
});
