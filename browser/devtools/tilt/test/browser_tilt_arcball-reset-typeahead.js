/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*global ok, is, info, isApproxVec, waitForExplicitFinish, executeSoon, finish */
/*global isTiltEnabled, isWebGLSupported, createTab, createTilt */
/*global Services, EventUtils, InspectorUI, TiltVisualizer, TILT_DESTROYED */
"use strict";

function test() {
  if (!isTiltEnabled()) {
    info("Skipping part of the arcball test because Tilt isn't enabled.");
    return;
  }
  if (!isWebGLSupported()) {
    info("Skipping part of the arcball test because WebGL isn't supported.");
    return;
  }

  requestLongerTimeout(10);
  waitForExplicitFinish();
  Services.prefs.setBoolPref("accessibility.typeaheadfind", true);

  createTab(function() {
    createTilt({
      onTiltOpen: function(instance)
      {
        performTest(instance.presenter.canvas,
                    instance.controller.arcball, function() {

          info("Killing arcball reset test.");

          Services.prefs.setBoolPref("accessibility.typeaheadfind", false);
          Services.obs.addObserver(cleanup, TILT_DESTROYED, false);
          InspectorUI.closeInspectorUI();
        });
      }
    });
  });
}

function performTest(canvas, arcball, callback) {
  is(document.activeElement, canvas,
    "The visualizer canvas should be focused when performing this test.");


  info("Starting arcball reset test.");

  // start translating and rotating sometime at random

  executeSoon(function() {
    info("Synthesizing key down events.");

    EventUtils.synthesizeKey("VK_W", { type: "keydown" });
    EventUtils.synthesizeKey("VK_LEFT", { type: "keydown" });

    // wait for some arcball translations and rotations to happen

    executeSoon(function() {
      info("Synthesizing key up events.");

      EventUtils.synthesizeKey("VK_W", { type: "keyup" });
      EventUtils.synthesizeKey("VK_LEFT", { type: "keyup" });

      // ok, transformations finished, we can now try to reset the model view

      executeSoon(function() {
        info("Synthesizing arcball reset key press.");

        arcball.onResetStart = function() {
          info("Starting arcball reset animation.");
        };

        arcball.onResetFinish = function() {
          ok(isApproxVec(arcball._lastRot, [0, 0, 0, 1]),
            "The arcball _lastRot field wasn't reset correctly.");
          ok(isApproxVec(arcball._deltaRot, [0, 0, 0, 1]),
            "The arcball _deltaRot field wasn't reset correctly.");
          ok(isApproxVec(arcball._currentRot, [0, 0, 0, 1]),
            "The arcball _currentRot field wasn't reset correctly.");

          ok(isApproxVec(arcball._lastTrans, [0, 0, 0]),
            "The arcball _lastTrans field wasn't reset correctly.");
          ok(isApproxVec(arcball._deltaTrans, [0, 0, 0]),
            "The arcball _deltaTrans field wasn't reset correctly.");
          ok(isApproxVec(arcball._currentTrans, [0, 0, 0]),
            "The arcball _currentTrans field wasn't reset correctly.");

          ok(isApproxVec(arcball._additionalRot, [0, 0, 0]),
            "The arcball _additionalRot field wasn't reset correctly.");
          ok(isApproxVec(arcball._additionalTrans, [0, 0, 0]),
            "The arcball _additionalTrans field wasn't reset correctly.");

          ok(isApproxVec([arcball._zoomAmount], [0]),
            "The arcball _zoomAmount field wasn't reset correctly.");

          info("Finishing arcball reset test.");
          callback();
        };

        EventUtils.synthesizeKey("VK_R", { type: "keydown" });
      });
    });
  });
}

function cleanup() { /*global gBrowser */
  info("Cleaning up arcball reset test.");

  Services.obs.removeObserver(cleanup, TILT_DESTROYED);
  gBrowser.removeCurrentTab();
  finish();
}
