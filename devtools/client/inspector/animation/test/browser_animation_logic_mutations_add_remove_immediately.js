/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether the animation inspector will not crash when add animation then remove
// immediately.

add_task(async function() {
  const tab = await addTab(
    URL_ROOT + "doc_mutations_add_remove_immediately.html"
  );
  const { inspector, panel } = await openAnimationInspector();

  info("Check state of the animation inspector after fast mutations");
  const onDispatch = waitForDispatch(inspector.store, "UPDATE_ANIMATIONS");
  await startMutation(tab);
  await onDispatch;
  ok(
    panel.querySelector(".animation-error-message"),
    "No animations message should display"
  );
});

async function startMutation(tab) {
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    await content.wrappedJSObject.startMutation();
  });
}
