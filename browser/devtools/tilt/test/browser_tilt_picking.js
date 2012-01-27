/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*global ok, is, info, waitForExplicitFinish, finish, gBrowser */
/*global isTiltEnabled, isWebGLSupported, createTab, createTilt */
/*global Services, InspectorUI, TILT_DESTROYED */
"use strict";

function test() {
  if (!isTiltEnabled()) {
    info("Skipping picking test because Tilt isn't enabled.");
    return;
  }
  if (!isWebGLSupported()) {
    info("Skipping picking test because WebGL isn't supported.");
    return;
  }

  waitForExplicitFinish();

  createTab(function() {
    createTilt({
      onTiltOpen: function(instance)
      {
        let presenter = instance.presenter;
        let canvas = presenter.canvas;

        presenter.onSetupMesh = function() {

          presenter.pickNode(canvas.width / 2, canvas.height / 2, {
            onpick: function(data)
            {
              ok(data.index > 0,
                "Simply picking a node didn't work properly.");
              ok(!presenter.highlight.disabled,
                "After only picking a node, it shouldn't be highlighted.");

              Services.obs.addObserver(cleanup, TILT_DESTROYED, false);
              InspectorUI.closeInspectorUI();
            }
          });
        };
      }
    });
  });
}

function cleanup() {
  Services.obs.removeObserver(cleanup, TILT_DESTROYED);
  gBrowser.removeCurrentTab();
  finish();
}
