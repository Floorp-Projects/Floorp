/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

let nodeDeleted = false;
let presenter;

function test() {
  if (!isTiltEnabled()) {
    info("Skipping picking delete test because Tilt isn't enabled.");
    return;
  }
  if (!isWebGLSupported()) {
    info("Skipping picking delete test because WebGL isn't supported.");
    return;
  }

  waitForExplicitFinish();

  createTab(function() {
    createTilt({
      onTiltOpen: function(instance)
      {
        presenter = instance.presenter;
        Services.obs.addObserver(whenNodeRemoved, NODE_REMOVED, false);

        presenter._onSetupMesh = function() {
          let p = getPickablePoint(presenter);

          presenter.highlightNodeAt(p[0], p[1], {
            onpick: function()
            {
              ok(presenter._currentSelection > 0,
                "Highlighting a node didn't work properly.");
              ok(!presenter._highlight.disabled,
                "After highlighting a node, it should be highlighted. D'oh.");

              nodeDeleted = true;
              presenter.deleteNode();
            }
          });
        };
      }
    }, false, function suddenDeath()
    {
      info("Tilt could not be initialized properly.");
      cleanup();
    });
  });
}

function whenNodeRemoved() {
  ok(presenter._currentSelection > 0,
    "Deleting a node shouldn't change the current selection.");
  ok(presenter._highlight.disabled,
    "After deleting a node, it shouldn't be highlighted.");

  let nodeIndex = presenter._currentSelection;
  let vertices = presenter._meshStacks[0].vertices.components;

  for (let i = 0, k = 36 * nodeIndex; i < 36; i++) {
    is(vertices[i + k], 0,
      "The stack vertices weren't degenerated properly.");
  }

  executeSoon(function() {
    Services.obs.addObserver(cleanup, DESTROYED, false);
    Tilt.destroy(Tilt.currentWindowId);
  });
}

function cleanup() {
  if (nodeDeleted) { Services.obs.removeObserver(cleanup, DESTROYED); }
  gBrowser.removeCurrentTab();
  finish();
}
