/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test top-level target switching in the DOM panel.

const PARENT_PROCESS_URI = "about:robots";
const CONTENT_PROCESS_URI = URL_ROOT + "page_basic.html";

add_task(async function() {
  // We use about:robots as the starting page because it will run in the parent process.
  // Navigating from that page to a regular content page will always trigger a target
  // switch, with or without fission.

  info("Open a page that runs in the parent process");
  const { panel } = await addTestTab(PARENT_PROCESS_URI);

  const _aProperty = getRowByLabel(panel, "_a");
  let buttonProperty = getRowByLabel(panel, "button");

  ok(!_aProperty, "There is no _a property on the about:robots page");
  ok(buttonProperty, "There is, however, a button property on this page");

  info("Navigate to a page that runs in the content process");
  // Wait for the DOM panel to refresh.
  const store = getReduxStoreFromPanel(panel);
  const onPropertiesFetched = waitForDispatch(store, "FETCH_PROPERTIES");
  // Also wait for the toolbox to switch to the new target, to avoid hanging requests when
  // the test ends.
  await navigateTo(CONTENT_PROCESS_URI);
  await onPropertiesFetched;

  await waitFor(() => getRowByLabel(panel, "_a"));
  ok(true, "This time, the _a property exists on this content process page");

  buttonProperty = getRowByLabel(panel, "button");
  ok(
    !buttonProperty,
    "There is, however, no more button property on this page"
  );
});
