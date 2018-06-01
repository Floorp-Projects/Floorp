/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the inspector splitter is properly initialized in horizontal mode if the
// inspector starts in portrait mode.

add_task(async function() {
  let { inspector, toolbox } = await openInspectorForURL(
    "data:text/html;charset=utf-8,<h1>foo</h1><span>bar</span>", "window");

  const hostWindow = toolbox.win.parent;
  const originalWidth = hostWindow.outerWidth;
  const originalHeight = hostWindow.outerHeight;

  let splitter = inspector.panelDoc.querySelector(".inspector-sidebar-splitter");

  // If the inspector is not already in landscape mode.
  if (!splitter.classList.contains("vert")) {
    info("Resize toolbox window to force inspector to landscape mode");
    const onClassnameMutation = waitForClassMutation(splitter);
    hostWindow.resizeTo(800, 500);
    await onClassnameMutation;

    ok(splitter.classList.contains("vert"), "Splitter is in vertical mode");
  }

  info("Resize toolbox window to force inspector to portrait mode");
  const onClassnameMutation = waitForClassMutation(splitter);
  hostWindow.resizeTo(500, 500);
  await onClassnameMutation;

  ok(splitter.classList.contains("horz"), "Splitter is in horizontal mode");

  info("Close the inspector");
  await gDevTools.closeToolbox(toolbox.target);

  info("Reopen inspector");
  ({ inspector, toolbox } = await openInspector("window"));

  // Devtools window should still be 500px * 500px, inspector should still be in portrait.
  splitter = inspector.panelDoc.querySelector(".inspector-sidebar-splitter");
  ok(splitter.classList.contains("horz"), "Splitter is in horizontal mode");

  info("Restore original window size");
  toolbox.win.parent.resizeTo(originalWidth, originalHeight);
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
    const observer = new MutationObserver((mutations) => {
      for (const mutation of mutations) {
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

registerCleanupFunction(function() {
  // Restore the host type for other tests.
  Services.prefs.clearUserPref("devtools.toolbox.host");
});
