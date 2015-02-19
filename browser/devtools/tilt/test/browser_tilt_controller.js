/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function test() {
  if (!isTiltEnabled()) {
    aborting();
    info("Skipping controller test because Tilt isn't enabled.");
    return;
  }
  if (!isWebGLSupported()) {
    aborting();
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

        function testEventCancel(cancellingEvent, cancellingDescription) {
          let description = "testEventCancel, cancellingEvent is " + cancellingDescription + ": ";
          is(document.activeElement, canvas,
             description + "The visualizer canvas should be focused when performing this test.");

          EventUtils.synthesizeKey("a", { type: "keydown", code: "KeyA", keyCode: KeyboardEvent.DOM_VK_A });
          instance.controller._update();
          ok(!isEqualVec(rot(), prev_rot),
             description + "After a rotation key is pressed, the quaternion should change.");
          EventUtils.synthesizeKey("a", { type: "keyup", code: "KeyA", keyCode: KeyboardEvent.DOM_VK_A });

          EventUtils.synthesizeKey("ArrowLeft", { type: "keydown", code: "ArrowLeft", keyCode: KeyboardEvent.DOM_VK_LEFT });
          instance.controller._update();
          ok(!isEqualVec(tran(), prev_tran),
             description + "After a translation key is pressed, the vector should change.");
          EventUtils.synthesizeKey("ArrowLeft", { type: "keyup", code: "ArrowLeft", keyCode: KeyboardEvent.DOM_VK_LEFT });

          save();


          cancellingEvent();
          instance.controller._update();

          ok(!isEqualVec(tran(), prev_tran),
             description + "Even if the canvas lost focus, the vector has some inertia.");
          ok(!isEqualVec(rot(), prev_rot),
             description + "Even if the canvas lost focus, the quaternion has some inertia.");

          save();


          while (!isEqualVec(tran(), prev_tran) ||
                 !isEqualVec(rot(), prev_rot)) {
            instance.controller._update();
            save();
          }

          ok(isEqualVec(tran(), prev_tran) && isEqualVec(rot(), prev_rot),
            "After focus lost, the transforms inertia eventually stops.");
        }

        info("Setting typeaheadfind to true.");

        let typeaheadfindEnabled = true;
        Services.prefs.setBoolPref("accessibility.typeaheadfind", typeaheadfindEnabled);
        for (var i = 0; i < 2; i++) {
          testEventCancel(function() {
            // XXX Don't use a character which is registered as a mnemonic in the menubar.
            EventUtils.synthesizeKey("A", { altKey: true, code: "KeyA", keyCode: KeyboardEvent.DOM_VK_A });
          }, "Alt + A");
          testEventCancel(function() {
            // XXX Don't use a character which is registered as a shortcut key.
            EventUtils.synthesizeKey(";", { ctrlKey: true, code: "Semicolon", keyCode: KeyboardEvent.DOM_VK_SEMICONLON });
          }, "Ctrl + ;");
          testEventCancel(function() {
            // XXX Don't use a character which is registered as a shortcut key.
            EventUtils.synthesizeKey("\\", { metaKey: true, code: "Backslash", keyCode: KeyboardEvent.DOM_VK_BACK_SLASH });
          }, "Meta + \\");
          // If typeahead is enabled, Shift + T causes moving focus to the findbar because it inputs "T".
          if (!typeaheadfindEnabled) {
            testEventCancel(function() {
              EventUtils.synthesizeKey("T", { shiftKey: true, code: "KeyT", keyCode: KeyboardEvent.DOM_VK_T });
            }, "Shift + T");
          }

          // Retry after disabling typeaheadfind.
          info("Setting typeaheadfind to false.");

          typeaheadfindEnabled = false;
          Services.prefs.setBoolPref("accessibility.typeaheadfind", typeaheadfindEnabled);
        }

        info("Testing if loosing focus halts any stacked arcball animations.");

        testEventCancel(function() {
          gBrowser.selectedBrowser.contentWindow.focus();
        }, "setting focus to the content window");
      },
      onEnd: function()
      {
        cleanup();
      }
    }, true, function suddenDeath()
    {
      ok(false, "Tilt could not be initialized properly.");
      cleanup();
    });
  });
}

function cleanup() {
  gBrowser.removeCurrentTab();
  finish();
}
