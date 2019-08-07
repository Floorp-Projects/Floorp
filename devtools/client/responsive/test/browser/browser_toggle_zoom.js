/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Verify full zoom levels inherit RDM full zoom after exiting RDM.

const TEST_URL = "http://example.com/";

function getZoomForBrowser(browser) {
  return ZoomManager.getZoomForBrowser(browser);
}

function setZoomForBrowser(browser, zoom) {
  ZoomManager.setZoomForBrowser(browser, zoom);
}

add_task(async function() {
  const INITIAL_ZOOM_LEVEL = 1;
  const PRE_RDM_ZOOM_LEVEL = 1.5;
  const MID_RDM_ZOOM_LEVEL = 2;

  const tab = await addTab(TEST_URL);
  const browser = tab.linkedBrowser;

  await load(browser, TEST_URL);

  // Get the initial zoom level.
  const initialOuterZoom = getZoomForBrowser(browser);
  is(
    initialOuterZoom,
    INITIAL_ZOOM_LEVEL,
    "Initial outer zoom should be " + INITIAL_ZOOM_LEVEL + "."
  );

  // Change the zoom level before we open RDM.
  setZoomForBrowser(browser, PRE_RDM_ZOOM_LEVEL);

  const preRDMOuterZoom = getZoomForBrowser(browser);
  is(
    preRDMOuterZoom,
    PRE_RDM_ZOOM_LEVEL,
    "Pre-RDM outer zoom should be " + PRE_RDM_ZOOM_LEVEL + "."
  );

  // Start RDM on the tab. This will fundamentally change the way that browser behaves.
  // It will now pass all of its messages through to the RDM docshell, meaning that when
  // we request zoom level from it now, we are getting the RDM zoom level.
  const { ui } = await openRDM(tab);
  const uiDocShell = ui.toolWindow.docShell;

  // Bug 1541692: openRDM behaves differently in the test harness than it does
  // interactively. Interactively, many features of the container docShell -- including
  // zoom -- are copied over to the RDM browser. In the test harness, this seems to first
  // reset the docShell before toggling RDM, which makes checking the initial zoom of the
  // RDM pane not useful.

  const preZoomUIZoom = uiDocShell.contentViewer.fullZoom;
  is(
    preZoomUIZoom,
    INITIAL_ZOOM_LEVEL,
    "Pre-zoom UI zoom should be " + INITIAL_ZOOM_LEVEL + "."
  );

  // Set the zoom level. This should tunnel to the inner browser and leave the UI alone.
  setZoomForBrowser(browser, MID_RDM_ZOOM_LEVEL);

  // The UI zoom should be unchanged by this.
  const postZoomUIZoom = uiDocShell.contentViewer.fullZoom;
  is(postZoomUIZoom, preZoomUIZoom, "UI zoom should be unchanged by RDM zoom.");

  // The RDM zoom should be changed.
  const finalRDMZoom = getZoomForBrowser(browser);
  is(
    finalRDMZoom,
    MID_RDM_ZOOM_LEVEL,
    "RDM zoom should be " + MID_RDM_ZOOM_LEVEL + "."
  );

  // Leave RDM. This should cause the outer pane to take on the full zoom of the RDM pane.
  await closeRDM(tab);

  // Bug 1541692: the following todo_is check will become an is check when this bug lands.
  const finalOuterZoom = getZoomForBrowser(browser);
  todo_is(
    finalOuterZoom,
    finalRDMZoom,
    "Final outer zoom should match last RDM zoom."
  );
});
