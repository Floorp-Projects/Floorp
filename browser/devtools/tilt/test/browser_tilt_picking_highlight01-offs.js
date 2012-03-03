/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

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

  requestLongerTimeout(10);
  waitForExplicitFinish();

  createTab(function() {
    createTilt({
      onTiltOpen: function(instance)
      {
        presenter = instance.presenter;
        Services.obs.addObserver(whenHighlighting, HIGHLIGHTING, false);

        presenter._onInitializationFinished = function() {
          let contentDocument = presenter.contentWindow.document;
          let div = contentDocument.getElementById("far-far-away");

          presenter.highlightNode(div, "moveIntoView");
        };
      }
    });
  });
}

function whenHighlighting() {
  ok(presenter._currentSelection > 0,
    "Highlighting a node didn't work properly.");
  ok(!presenter._highlight.disabled,
    "After highlighting a node, it should be highlighted. D'oh.");
  ok(presenter.controller.arcball._resetInProgress,
    "Highlighting a node that's not already visible should trigger a reset!");

  executeSoon(function() {
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
    Services.obs.addObserver(cleanup, DESTROYED, false);
    InspectorUI.closeInspectorUI();
  });
}

function cleanup() {
  Services.obs.removeObserver(whenHighlighting, HIGHLIGHTING);
  Services.obs.removeObserver(whenUnhighlighting, UNHIGHLIGHTING);
  Services.obs.removeObserver(cleanup, DESTROYED);
  gBrowser.removeCurrentTab();
  finish();
}
