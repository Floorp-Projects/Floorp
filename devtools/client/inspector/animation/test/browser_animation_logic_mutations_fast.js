/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether the animation inspector will not crash when remove/add animations faster.

add_task(async function() {
  const tab = await addTab(URL_ROOT + "doc_mutations_fast.html");
  const { inspector } = await openAnimationInspector();

  info("Check state of the animation inspector after fast mutations");
  await startFastMutations(tab);
  ok(inspector.panelWin.document.getElementById("animation-container"),
    "Animation inspector should be live");
});

async function startFastMutations(tab) {
  await ContentTask.spawn(tab.linkedBrowser, {}, async function() {
    await content.wrappedJSObject.startFastMutations();
  });
}
