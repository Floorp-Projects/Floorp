/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

let nodeHighlighted = false;
let presenter;

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
        presenter = instance.presenter;
        Services.obs.addObserver(whenHighlighting, HIGHLIGHTING, false);

        presenter._onInitializationFinished = function() {
          nodeHighlighted = true;
          presenter.highlightNodeAt.apply(this, getPickablePoint(presenter));
        };
      }
    }, false, function suddenDeath()
    {
      info("Tilt could not be initialized properly.");
      cleanup();
    });
  });
}

function whenHighlighting() {
  ok(presenter._currentSelection > 0,
    "Highlighting a node didn't work properly.");
  ok(!presenter._highlight.disabled,
    "After highlighting a node, it should be highlighted. D'oh.");

  executeSoon(function() {
    Services.obs.removeObserver(whenHighlighting, HIGHLIGHTING);
    Services.obs.addObserver(whenUnhighlighting, UNHIGHLIGHTING, false);
    presenter.highlightNode(null);
  });
}

function whenUnhighlighting() {
  ok(presenter._currentSelection < 0,
    "Unhighlighting a should remove the current selection.");
  ok(presenter._highlight.disabled,
    "After unhighlighting a node, it shouldn't be highlighted anymore. D'oh.");

  executeSoon(function() {
    Services.obs.removeObserver(whenUnhighlighting, UNHIGHLIGHTING);
    Services.obs.addObserver(cleanup, DESTROYED, false);
    InspectorUI.closeInspectorUI();
  });
}

function cleanup() {
  if (nodeHighlighted) { Services.obs.removeObserver(cleanup, DESTROYED); }
  gBrowser.removeCurrentTab();
  finish();
}
