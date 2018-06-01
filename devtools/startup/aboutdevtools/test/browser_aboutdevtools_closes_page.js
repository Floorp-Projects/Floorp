/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* eslint-env browser */

add_task(async function() {
  pushPref("devtools.enabled", false);

  const {doc, win} = await openAboutDevTools();

  info("Check that the close button is available on the page");
  const closeButton = doc.getElementById("close");
  ok(closeButton, "close button is displayed");

  const onWindowUnload =
    new Promise(r => win.addEventListener("unload", r, {once: true}));
  info("Click on the install button to enable DevTools.");
  EventUtils.synthesizeMouseAtCenter(closeButton, {}, win);

  info("Wait for the about:devtools tab to be closed");
  await onWindowUnload;
});
