/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether the animation inspector will not crash when remove/add animations faster.

add_task(async function() {
  const tab = await addTab(URL_ROOT + "doc_mutations_fast.html");
  const { inspector } = await openAnimationInspector();

  info("Check state of the animation inspector after fast mutations");
  const animationsFinished = waitForAnimations(inspector);
  await startFastMutations(tab);
  ok(inspector.panelWin.document.getElementById("animation-container"),
    "Animation inspector should be live");
  await animationsFinished;
});

async function startFastMutations(tab) {
  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    await content.wrappedJSObject.startFastMutations();
  });
}

function waitForAnimations(inspector) {
  // wait at least once
  let count = 1;
  // queue any further waits
  inspector.animationinspector.animationsFront.on("mutations", () => count++);
  return waitForDispatch(inspector, "UPDATE_ANIMATIONS", () => count);
}
