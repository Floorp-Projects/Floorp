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

          presenter.highlightNode(div);
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
  ok(!presenter.controller.arcball._resetInProgress,
    "Highlighting a node that's not already visible shouldn't trigger a reset " +
    "without this being explicitly requested!");

  EventUtils.sendKey("F");
  executeSoon(whenBringingIntoView);
}

function whenBringingIntoView() {
  ok(presenter._currentSelection > 0,
    "The node should still be selected.");
  ok(!presenter._highlight.disabled,
    "The node should still be highlighted");
  ok(presenter.controller.arcball._resetInProgress,
    "Highlighting a node that's not already visible should trigger a reset " +
    "when this is being explicitly requested!");

  executeSoon(function() {
    Services.obs.addObserver(cleanup, DESTROYED, false);
    InspectorUI.closeInspectorUI();
  });
}

function cleanup() {
  Services.obs.removeObserver(whenHighlighting, HIGHLIGHTING);
  Services.obs.removeObserver(cleanup, DESTROYED);
  gBrowser.removeCurrentTab();
  finish();
}
