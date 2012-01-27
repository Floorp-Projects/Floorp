/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*global ok, is, info, waitForExplicitFinish, finish, gBrowser */
/*global isTiltEnabled, isWebGLSupported, createTab, createTilt */
/*global Services, InspectorUI, TILT_DESTROYED */
"use strict";

function test() {
  if (!isTiltEnabled()) {
    info("Skipping highlight test because Tilt isn't enabled.");
    return;
  }
  if (!isWebGLSupported()) {
    info("Skipping highlight test because WebGL isn't supported.");
    return;
  }

  waitForExplicitFinish();

  createTab(function() {
    createTilt({
      onTiltOpen: function(instance)
      {
        let presenter = instance.presenter;

        presenter.onSetupMesh = function() {
          presenter.highlightNodeFor(1);

          ok(presenter._currentSelection > 0,
            "Highlighting a node didn't work properly.");
          ok(!presenter.highlight.disabled,
            "After highlighting a node, it should be highlighted. D'oh.");

          Services.obs.addObserver(cleanup, TILT_DESTROYED, false);
          InspectorUI.closeInspectorUI();
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
