/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

registerCleanupFunction(async function() {
  await task_resetState();
  await PlacesUtils.history.clear();
});

add_task(async function test_indicatorDrop() {
  let EventUtils = {};
  Services.scriptloader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", EventUtils);

  // Ensure that state is reset in case previous tests didn't finish.
  await task_resetState();

  await setDownloadDir();

  startServer();

  let win = await openLibrary("Downloads");
  registerCleanupFunction(function() {
    win.close();
  });

  let listBox = win.document.getElementById("downloadsRichListBox");
  ok(listBox, "download list box present");

  await simulateDropAndCheck(win, listBox, [httpUrl("file1.txt")]);
  await simulateDropAndCheck(win, listBox, [
    httpUrl("file1.txt"),
    httpUrl("file2.txt"),
    httpUrl("file3.txt"),
  ]);
});
