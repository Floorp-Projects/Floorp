/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

let pickDone = false;

function test() {
  if (!isTiltEnabled()) {
    aborting();
    info("Skipping picking test because Tilt isn't enabled.");
    return;
  }
  if (!isWebGLSupported()) {
    aborting();
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

        presenter._onSetupMesh = function() {
          let p = getPickablePoint(presenter);

          presenter.pickNode(p[0], p[1], {
            onpick: function(data)
            {
              ok(data.index > 0,
                "Simply picking a node didn't work properly.");

              pickDone = true;
              Services.obs.addObserver(cleanup, DESTROYED, false);
              Tilt.destroy(Tilt.currentWindowId);
            }
          });
        };
      }
    }, false, function suddenDeath()
    {
      ok(false, "Tilt could not be initialized properly.");
      cleanup();
    });
  });
}

function cleanup() {
  if (pickDone) { Services.obs.removeObserver(cleanup, DESTROYED); }
  gBrowser.removeCurrentTab();
  finish();
}
