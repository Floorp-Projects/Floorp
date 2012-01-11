/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*global ok, is, info, waitForExplicitFinish, finish, gBrowser */
/*global isTiltEnabled, isWebGLSupported, createTab, createTilt */
/*global Tilt, TiltUtils, TiltVisualizer */
"use strict";

function test() {
  if (!isTiltEnabled()) {
    info("Skipping initialization test because Tilt isn't enabled.");
    return;
  }
  if (!isWebGLSupported()) {
    info("Skipping initialization test because WebGL isn't supported.");
    return;
  }

  waitForExplicitFinish();

  createTab(function() {
    let id = TiltUtils.getWindowId(gBrowser.selectedBrowser.contentWindow);
    let initialActiveElement;

    is(id, Tilt.currentWindowId,
      "The unique window identifiers should match for the same window.");

    createTilt({
      onInspectorOpen: function() {
        initialActiveElement = document.activeElement;

        is(Tilt.visualizers[id], null,
          "A instance of the visualizer shouldn't be initialized yet.");

        is(typeof TiltVisualizer.Prefs.enabled, "boolean",
          "The 'enabled' pref should have been loaded by now.");
        is(typeof TiltVisualizer.Prefs.forceEnabled, "boolean",
          "The 'force-enabled' pref should have been loaded by now.");
      },
      onTiltOpen: function(instance)
      {
        is(document.activeElement, instance.presenter.canvas,
          "The visualizer canvas should be focused on initialization.");

        ok(Tilt.visualizers[id] instanceof TiltVisualizer,
          "A new instance of the visualizer wasn't created properly.");
        ok(Tilt.visualizers[id].isInitialized(),
          "The new instance of the visualizer wasn't initialized properly.");
      },
      onTiltClose: function()
      {
        is(document.activeElement, initialActiveElement,
          "The focus wasn't correctly given back to the initial element.");

        is(Tilt.visualizers[id], null,
          "The current instance of the visualizer wasn't destroyed properly.");
      },
      onEnd: function()
      {
        gBrowser.removeCurrentTab();
        finish();
      }
    }, true);
  });
}
