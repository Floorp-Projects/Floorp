/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Test that top-level net monitor error boundary catches child errors.
 */
add_task(async function () {
  const { monitor } = await initNetMonitor(SIMPLE_URL, {
    requestCount: 1,
  });

  const { store, windowRequire, document } = monitor.panelWin;
  const Actions = windowRequire("devtools/client/netmonitor/src/actions/index");

  store.dispatch(Actions.batchEnable(false));

  // Intentionally damage the store to cause a child component error
  const state = store.getState();
  state.ui.columns = null;

  await reloadBrowser();

  // Wait for the panel to fall back to the error UI
  const errorPanel = await waitUntil(() =>
    document.querySelector(".app-error-panel")
  );

  is(errorPanel, !undefined);
  return teardown(monitor);
});
