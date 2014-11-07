/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function test() {
  if (!isTiltEnabled()) {
    aborting();
    info("Skipping notifications test because Tilt isn't enabled.");
    return;
  }
  if (!isWebGLSupported()) {
    aborting();
    info("Skipping visualizer test because WebGL isn't supported.");
    return;
  }

  let webGLError = false;
  let webGLLoad = false;

  let visualizer = new TiltVisualizer({
    chromeWindow: window,
    contentWindow: gBrowser.selectedBrowser.contentWindow,
    parentNode: gBrowser.selectedBrowser.parentNode,
    notifications: Tilt.NOTIFICATIONS,
    tab: gBrowser.selectedTab,

    onError: function onWebGLError()
    {
      webGLError = true;
    },

    onLoad: function onWebGLLoad()
    {
      webGLLoad = true;
    }
  });
  visualizer.init();

  ok(webGLError ^ webGLLoad,
    "The WebGL context should either be created or not.");

  if (webGLError) {
    info("Skipping visualizer test because WebGL couldn't be initialized.");
    return;
  }

  ok(visualizer.canvas,
    "Visualizer constructor should have created a child canvas object.");
  ok(visualizer.presenter,
    "Visualizer constructor should have created a child presenter object.");
  ok(visualizer.controller,
    "Visualizer constructor should have created a child controller object.");
  ok(visualizer.isInitialized(),
    "The visualizer should have been initialized properly.");
  ok(visualizer.presenter.isInitialized(),
    "The visualizer presenter should have been initialized properly.");
  ok(visualizer.controller.isInitialized(),
    "The visualizer controller should have been initialized properly.");

  testPresenter(visualizer.presenter);
  testController(visualizer.controller);

  visualizer.removeOverlay();
  is(visualizer.canvas.parentNode, null,
    "The visualizer canvas wasn't removed from the parent node.");

  visualizer.cleanup();
  is(visualizer.presenter, undefined,
    "The visualizer presenter wasn't destroyed.");
  is(visualizer.controller, undefined,
    "The visualizer controller wasn't destroyed.");
  is(visualizer.canvas, undefined,
    "The visualizer canvas wasn't destroyed.");
}

function testPresenter(presenter) {
  ok(presenter._renderer,
    "The presenter renderer wasn't initialized properly.");
  ok(presenter._visualizationProgram,
    "The presenter visualizationProgram wasn't initialized properly.");
  ok(presenter._texture,
    "The presenter texture wasn't initialized properly.");
  ok(!presenter._meshStacks,
    "The presenter meshStacks shouldn't be initialized yet.");
  ok(!presenter._meshWireframe,
    "The presenter meshWireframe shouldn't be initialized yet.");
  ok(presenter._traverseData,
    "The presenter nodesInformation wasn't initialized properly.");
  ok(presenter._highlight,
    "The presenter highlight wasn't initialized properly.");
  ok(presenter._highlight.disabled,
    "The presenter highlight should be initially disabled.");
  ok(isApproxVec(presenter._highlight.v0, [0, 0, 0]),
    "The presenter highlight first vertex should be initially zeroed.");
  ok(isApproxVec(presenter._highlight.v1, [0, 0, 0]),
    "The presenter highlight second vertex should be initially zeroed.");
  ok(isApproxVec(presenter._highlight.v2, [0, 0, 0]),
    "The presenter highlight third vertex should be initially zeroed.");
  ok(isApproxVec(presenter._highlight.v3, [0, 0, 0]),
    "The presenter highlight fourth vertex should be initially zeroed.");
  ok(presenter.transforms,
    "The presenter transforms wasn't initialized properly.");
  is(presenter.transforms.zoom, 1,
    "The presenter transforms zoom should be initially 1.");
  ok(isApproxVec(presenter.transforms.offset, [0, 0, 0]),
    "The presenter transforms offset should be initially zeroed.");
  ok(isApproxVec(presenter.transforms.translation, [0, 0, 0]),
    "The presenter transforms translation should be initially zeroed.");
  ok(isApproxVec(presenter.transforms.rotation, [0, 0, 0, 1]),
    "The presenter transforms rotation should be initially set to identity.");

  presenter.setTranslation([1, 2, 3]);
  presenter.setRotation([5, 6, 7, 8]);

  ok(isApproxVec(presenter.transforms.translation, [1, 2, 3]),
    "The presenter transforms translation wasn't modified as it should");
  ok(isApproxVec(presenter.transforms.rotation, [5, 6, 7, 8]),
    "The presenter transforms rotation wasn't modified as it should");
  ok(presenter._redraw,
    "The new transforms should have issued a redraw request.");
}

function testController(controller) {
  ok(controller.arcball,
    "The controller arcball wasn't initialized properly.");
  ok(!controller.coordinates,
    "The presenter meshWireframe shouldn't be initialized yet.");
}
