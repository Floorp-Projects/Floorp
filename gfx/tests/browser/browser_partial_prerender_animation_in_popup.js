/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../../dom/animation/test/testcommon.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/tests/dom/animation/test/testcommon.js",
  this
);

add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["layout.animation.prerender.partial", true],
      ["layout.animation.prerender.viewport-ratio-limit", 1.125],
    ],
  });

  let navBar = document.getElementById("nav-bar");

  const anchor = document.createXULElement("toolbarbutton");
  anchor.classList.add("toolbarbutton-1", "chromeclass-toolbar-additional");
  navBar.appendChild(anchor);

  // Prepare a popup panel.
  const panel = document.createXULElement("panel");
  panel.setAttribute("noautohide", true);
  navBar.appendChild(panel);

  // Add a overflow:scroll container to avoid expanding the popup window size.
  const container = document.createElement("div");
  container.style = "width: 100px; height: 100px; overflow: scroll;";

  // Looks like in popup window wider elements in the overflow:scroll container
  // are still affecting the viewport size of the popup content, for example,
  // if we specify "witdh: 800px" here, this element is not going to be partial-
  // prerendered, it will be fully prerendered, so we use vertically overflowed
  // element here.
  const target = document.createElement("div");
  target.style = "width: 100px; height: 800px;";

  container.appendChild(target);
  panel.appendChild(container);

  registerCleanupFunction(() => {
    panel.remove();
    anchor.remove();
  });

  panel.openPopup(anchor);

  // Start a transform transition with a 1s delay step-start function so that
  // we can ensure that
  // 1) when the target element is initially composited on the compositor the
  //    transition hasn't yet started, thus no jank happens
  // 2) when the transition starts on the compositor thread, it causes a jank
  //    so that it will report back to the main-thread
  target.style.transition = "transform 100s step-start 1s";
  getComputedStyle(target);
  const startTransition = new Promise(resolve => {
    target.addEventListener("transitionstart", resolve);
  });
  target.style.transform = "translateY(-130px)";
  const transition = target.getAnimations()[0];

  // Wait for the transition start.
  await startTransition;

  // Make sure it's running on the compositor.
  Assert.ok(
    transition.isRunningOnCompositor,
    "The transition should be running on the compositor thread"
  );

  // Collect restyling markers in 5 frames.
  const restyleCount = await observeStylingInTargetWindow(panel.ownerGlobal, 5);

  // On non WebRender we observe two restyling markers because we get the second
  // jank report from the compositor thread before a new pre-rendered result,
  // which was triggered by the first jank report, reached to the compositor
  // thread. So we allow one or two makers here.
  // NOTE: Since we wrap the target element in overflow:scroll container, we
  // might see an additional restyling marker triggered by
  // KeyframeEffect::OverflowRegionRefreshInterval (200ms) on very slow
  // platforms (e.g. TSAN builds), if it happens we should allow the additional
  // restyling here.
  Assert.greaterOrEqual(restyleCount, 1);
  Assert.lessOrEqual(restyleCount, 2);
});
