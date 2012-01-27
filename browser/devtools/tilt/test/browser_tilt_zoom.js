/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*global ok, is, info, waitForExplicitFinish, finish, executeSoon, gBrowser */
/*global isApprox, isTiltEnabled, isWebGLSupported, createTab, createTilt */
/*global Services, EventUtils, TiltUtils, InspectorUI, TILT_DESTROYED */
"use strict";

const ZOOM = 2;
const RESIZE = 50;

function setZoom(value) {
  gBrowser.selectedBrowser.markupDocumentViewer.fullZoom = value;
}

function getZoom() {
  return gBrowser.selectedBrowser.markupDocumentViewer.fullZoom;
}

function test() {
  TiltUtils.setDocumentZoom(Math.random());
  is(getZoom(), TiltUtils.getDocumentZoom(),
    "The getDocumentZoom utility function didn't return the expected results.");


  if (!isTiltEnabled()) {
    info("Skipping controller test because Tilt isn't enabled.");
    return;
  }
  if (!isWebGLSupported()) {
    info("Skipping controller test because WebGL isn't supported.");
    return;
  }

  waitForExplicitFinish();

  createTab(function() {
    createTilt({
      onInspectorOpen: function()
      {
        setZoom(ZOOM);
      },
      onTiltOpen: function(instance)
      {
        ok(isApprox(instance.presenter.transforms.zoom, ZOOM),
          "The presenter transforms zoom wasn't initially set correctly.");

        let contentWindow = gBrowser.selectedBrowser.contentWindow;
        let initialWidth = contentWindow.innerWidth;
        let initialHeight = contentWindow.innerHeight;

        let renderer = instance.presenter.renderer;
        let arcball = instance.controller.arcball;

        ok(isApprox(contentWindow.innerWidth * ZOOM, renderer.width, 1),
          "The renderer width wasn't set correctly.");
        ok(isApprox(contentWindow.innerHeight * ZOOM, renderer.height, 1),
          "The renderer height wasn't set correctly.");

        ok(isApprox(contentWindow.innerWidth * ZOOM, arcball.width, 1),
          "The arcball width wasn't set correctly.");
        ok(isApprox(contentWindow.innerHeight * ZOOM, arcball.height, 1),
          "The arcball height wasn't set correctly.");


        window.resizeBy(-RESIZE * ZOOM, -RESIZE * ZOOM);

        executeSoon(function() {
          ok(isApprox(contentWindow.innerWidth + RESIZE, initialWidth, 1),
            "The content window width wasn't set correctly.");
          ok(isApprox(contentWindow.innerHeight + RESIZE, initialHeight, 1),
            "The content window height wasn't set correctly.");

          ok(isApprox(contentWindow.innerWidth * ZOOM, renderer.width, 1),
            "The renderer width wasn't set correctly.");
          ok(isApprox(contentWindow.innerHeight * ZOOM, renderer.height, 1),
            "The renderer height wasn't set correctly.");

          ok(isApprox(contentWindow.innerWidth * ZOOM, arcball.width, 1),
            "The arcball width wasn't set correctly.");
          ok(isApprox(contentWindow.innerHeight * ZOOM, arcball.height, 1),
            "The arcball height wasn't set correctly.");


          window.resizeBy(RESIZE * ZOOM, RESIZE * ZOOM);

          Services.obs.addObserver(cleanup, TILT_DESTROYED, false);
          InspectorUI.closeInspectorUI();
        });
      },
    });
  });
}

function cleanup() {
  Services.obs.removeObserver(cleanup, TILT_DESTROYED);
  gBrowser.removeCurrentTab();
  finish();
}
