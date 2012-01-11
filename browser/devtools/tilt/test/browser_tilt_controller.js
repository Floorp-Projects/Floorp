/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*global ok, is, info, waitForExplicitFinish, finish, executeSoon, gBrowser */
/*global isEqualVec, isTiltEnabled, isWebGLSupported, createTab, createTilt */
/*global EventUtils, vec3, mat4, quat4 */
"use strict";

function test() {
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
      onTiltOpen: function(instance)
      {
        let canvas = instance.presenter.canvas;
        let prev_tran = vec3.create([0, 0, 0]);
        let prev_rot = quat4.create([0, 0, 0, 1]);

        function tran() {
          return instance.presenter.transforms.translation;
        }

        function rot() {
          return instance.presenter.transforms.rotation;
        }

        function save() {
          prev_tran = vec3.create(tran());
          prev_rot = quat4.create(rot());
        }

        ok(isEqualVec(tran(), prev_tran),
          "At init, the translation should be zero.");
        ok(isEqualVec(rot(), prev_rot),
          "At init, the rotation should be zero.");


        function testEventCancel(cancellingEvent) {
          EventUtils.synthesizeKey("VK_A", { type: "keydown" });
          EventUtils.synthesizeKey("VK_LEFT", { type: "keydown" });
          instance.controller.update();

          ok(!isEqualVec(tran(), prev_tran),
            "After a translation key is pressed, the vector should change.");
          ok(!isEqualVec(rot(), prev_rot),
            "After a rotation key is pressed, the quaternion should change.");

          save();


          cancellingEvent();
          instance.controller.update();

          ok(!isEqualVec(tran(), prev_tran),
            "Even if the canvas lost focus, the vector has some inertia.");
          ok(!isEqualVec(rot(), prev_rot),
            "Even if the canvas lost focus, the quaternion has some inertia.");

          save();


          while (!isEqualVec(tran(), prev_tran) ||
                 !isEqualVec(rot(), prev_rot)) {
            instance.controller.update();
            save();
          }

          ok(isEqualVec(tran(), prev_tran) && isEqualVec(rot(), prev_rot),
            "After focus lost, the transforms inertia eventually stops.");
        }

        testEventCancel(function() {
          EventUtils.synthesizeKey("T", { type: "keydown", altKey: 1 });
        });
        testEventCancel(function() {
          EventUtils.synthesizeKey("I", { type: "keydown", ctrlKey: 1 });
        });
        testEventCancel(function() {
          EventUtils.synthesizeKey("L", { type: "keydown", metaKey: 1 });
        });
        testEventCancel(function() {
          EventUtils.synthesizeKey("T", { type: "keydown", shiftKey: 1 });
        });
        testEventCancel(function() {
          gBrowser.selectedBrowser.contentWindow.focus();
        });
      },
      onEnd: function()
      {
        gBrowser.removeCurrentTab();
        finish();
      }
    }, true);
  });
}
