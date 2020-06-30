/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test() {
  info(
    "Test that the profiler popup gets disabled when a private browsing window is open."
  );
  await makeSureProfilerPopupIsEnabled();

  const getRecordingButton = () =>
    getElementByLabel(document, "Start Recording");
  const getDisabledMessage = () =>
    getElementFromDocumentByText(
      document,
      "The profiler is currently disabled"
    );

  await withPopupOpen(window, async () => {
    ok(await getRecordingButton(), "The start recording button is available");
  });

  info("Open a private browsing window.");
  const privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  info("Switch back to the main window and open the popup again.");
  window.focus();
  await withPopupOpen(window, async () => {
    ok(await getDisabledMessage(), "The disabled message is displayed.");
  });

  info("Close the private window");
  await BrowserTestUtils.closeWindow(privateWindow);

  info("Make sure the first window is focused, and open the popup back up.");
  window.focus();
  await withPopupOpen(window, async () => {
    ok(
      await getRecordingButton(),
      "The start recording button is available once again."
    );
  });
});
