/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
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

  createTab(function() {
    createTilt({
      onTiltOpen: function(instance)
      {
        performTest(instance.presenter.canvas,
                    instance.controller.arcball, function() {

          info("Killing arcball reset test.");

          Services.obs.addObserver(cleanup, DESTROYED, false);
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

  window.setTimeout(function() {
    info("Synthesizing key down events.");

    EventUtils.synthesizeKey("VK_W", { type: "keydown" });
    EventUtils.synthesizeKey("VK_LEFT", { type: "keydown" });

    // wait for some arcball translations and rotations to happen

    window.setTimeout(function() {
      info("Synthesizing key up events.");

      EventUtils.synthesizeKey("VK_W", { type: "keyup" });
      EventUtils.synthesizeKey("VK_LEFT", { type: "keyup" });

      // ok, transformations finished, we can now try to reset the model view

      window.setTimeout(function() {
        info("Synthesizing arcball reset key press.");

        arcball._onResetStart = function() {
          info("Starting arcball reset animation.");
        };

        arcball._onResetStep = function() {
          info("\nlastRot: " + quat4.str(arcball._lastRot) +
               "\ndeltaRot: " + quat4.str(arcball._deltaRot) +
               "\ncurrentRot: " + quat4.str(arcball._currentRot) +
               "\nlastTrans: " + vec3.str(arcball._lastTrans) +
               "\ndeltaTrans: " + vec3.str(arcball._deltaTrans) +
               "\ncurrentTrans: " + vec3.str(arcball._currentTrans) +
               "\nadditionalRot: " + vec3.str(arcball._additionalRot) +
               "\nadditionalTrans: " + vec3.str(arcball._additionalTrans) +
               "\nzoomAmount: " + arcball._zoomAmount);
        };

        arcball._onResetFinish = function() {
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

          executeSoon(function() {
            info("Finishing arcball reset test.");
            callback();
          });
        };

        EventUtils.synthesizeKey("VK_R", { type: "keydown" });

      }, Math.random() * 1000); // leave enough time for transforms to happen
    }, Math.random() * 1000);
  }, Math.random() * 1000);
}

function cleanup() {
  info("Cleaning up arcball reset test.");

  Services.obs.removeObserver(cleanup, DESTROYED);
  gBrowser.removeCurrentTab();
  finish();
}
