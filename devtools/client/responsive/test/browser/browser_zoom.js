/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL = "data:text/html,foo";

addRDMTaskWithPreAndPost(
  URL,
  async function pre({ browser }) {
    info("Setting zoom");
    // It's important that we do this so that we don't race with FullZoom's use
    // of ContentSettings, which would reset the zoom.
    FullZoom.setZoom(2.0, browser);
  },
  async function task({ browser, ui }) {
    is(
      ZoomManager.getZoomForBrowser(browser),
      2.0,
      "Zoom shouldn't have got lost"
    );

    // wait for the list of devices to be loaded to prevent pending promises
    await waitForDeviceAndViewportState(ui);
  },
  async function post() {}
);
