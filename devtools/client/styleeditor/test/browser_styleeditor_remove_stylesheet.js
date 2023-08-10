/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that a removed style sheet don't show up in the style editor anymore.

const TESTCASE_URI = TEST_BASE_HTTPS + "simple.html";

add_task(async function () {
  const { ui } = await openStyleEditorForURL(TESTCASE_URI);

  is(ui.editors.length, 2, "Two sheets present after load.");

  info("Removing the <style> node");
  const onStyleRemoved = waitFor(() => ui.editors.length == 1);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.document.querySelector("style").remove();
  });
  await onStyleRemoved;
  is(ui.editors.length, 1, "There's only one stylesheet remaining");

  info("Removing the <link> node");
  const onLinkRemoved = waitFor(() => !ui.editors.length);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.document.querySelector("link").remove();
  });
  await onLinkRemoved;
  is(ui.editors.length, 0, "There's no stylesheet displayed anymore");
});
