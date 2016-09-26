/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the inspector splitter is properly initialized in horizontal mode if the
// inspector starts in portrait mode.

add_task(function* () {
  let { inspector, toolbox } = yield openInspectorForURL(
    "data:text/html;charset=utf-8,<h1>foo</h1><span>bar</span>", "window");

  let hostWindow = toolbox._host._window;
  let originalWidth = hostWindow.outerWidth;
  let originalHeight = hostWindow.outerHeight;

  let splitter = inspector.panelDoc.querySelector(".inspector-sidebar-splitter");

  // If the inspector is not already in landscape mode.
  if (!splitter.classList.contains("vert")) {
    info("Resize toolbox window to force inspector to landscape mode");
    let onClassnameMutation = waitForClassMutation(splitter);
    hostWindow.resizeTo(800, 500);
    yield onClassnameMutation;

    ok(splitter.classList.contains("vert"), "Splitter is in vertical mode");
  }

  info("Resize toolbox window to force inspector to portrait mode");
  let onClassnameMutation = waitForClassMutation(splitter);
  hostWindow.resizeTo(500, 500);
  yield onClassnameMutation;

  ok(splitter.classList.contains("horz"), "Splitter is in horizontal mode");

  info("Close the inspector");
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  yield gDevTools.closeToolbox(target);

  info("Reopen inspector");
  ({ inspector, toolbox } = yield openInspector("window"));

  // Devtools window should still be 500px * 500px, inspector should still be in portrait.
  splitter = inspector.panelDoc.querySelector(".inspector-sidebar-splitter");
  ok(splitter.classList.contains("horz"), "Splitter is in horizontal mode");

  info("Restore original window size");
  toolbox._host._window.resizeTo(originalWidth, originalHeight);
});

/**
 * Helper waiting for a class attribute mutation on the provided target. Returns a
 * promise.
 *
 * @param {Node} target
 *        Node to observe
 * @return {Promise} promise that will resolve upon receiving a mutation for the class
 *         attribute on the target.
 */
function waitForClassMutation(target) {
  return new Promise(resolve => {
    let observer = new MutationObserver((mutations) => {
      for (let mutation of mutations) {
        if (mutation.attributeName === "class") {
          observer.disconnect();
          resolve();
          return;
        }
      }
    });
    observer.observe(target, { attributes: true });
  });
}

registerCleanupFunction(function () {
  // Restore the host type for other tests.
  Services.prefs.clearUserPref("devtools.toolbox.host");
});
