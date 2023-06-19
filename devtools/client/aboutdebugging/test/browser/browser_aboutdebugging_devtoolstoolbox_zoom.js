/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from helper-collapsibilities.js */
Services.scriptloader.loadSubScript(
  CHROME_URL_ROOT + "helper-collapsibilities.js",
  this
);

const L10N = new LocalizationHelper(
  "devtools/client/locales/toolbox.properties"
);

// Check that the about:devtools-toolbox tab can be zoomed in and that the zoom
// persists after switching tabs.
add_task(async function () {
  info("Force all debug target panes to be expanded");
  prepareCollapsibilitiesTest();

  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);
  const { devtoolsTab, devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window
  );

  is(getZoom(devtoolsWindow), 1, "default zoom level correct");

  info("Increase the zoom level");

  // Note that we read the shortcut from the toolbox properties, but that should
  // match the default browser shortcut `full-zoom-enlarge-shortcut`.
  synthesizeKeyShortcut(L10N.getStr("toolbox.zoomIn.key"));
  await waitFor(() => getZoom(devtoolsWindow) > 1);
  is(getZoom(devtoolsWindow).toFixed(2), "1.10", "zoom level increased");

  info("Switch tabs between about:debugging and the toolbox tab");
  gBrowser.selectedTab = tab;
  gBrowser.selectedTab = devtoolsTab;

  info("Wait for the browser to reapply the zoom");
  await wait(500);

  is(
    getZoom(devtoolsWindow).toFixed(2),
    "1.10",
    "zoom level was restored after switching tabs"
  );

  info("Restore the default zoom level");
  synthesizeKeyShortcut(L10N.getStr("toolbox.zoomReset.key"));
  await waitFor(() => getZoom(devtoolsWindow) === 1);
  is(getZoom(devtoolsWindow), 1, "default zoom level restored");

  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);
  await removeTab(tab);
});

function getZoom(win) {
  return win.browsingContext.fullZoom;
}
