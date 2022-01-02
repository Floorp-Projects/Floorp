/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function remove_subframe_in_cross_site_frame() {
  await BrowserTestUtils.withNewTab(
    "http://mochi.test:8888/browser/dom/tests/browser/file_empty_cross_site_frame.html",
    async browser => {
      await TestUtils.waitForCondition(
        () => !XULBrowserWindow.isBusy,
        "browser is not busy after the tab finishes loading"
      );

      // Spawn into the cross-site subframe, and begin loading a slow network
      // connection. We'll cancel the load before this navigation completes.
      await SpecialPowers.spawn(
        browser.browsingContext.children[0],
        [],
        async () => {
          let frame = content.document.createElement("iframe");
          frame.src = "load_forever.sjs";
          content.document.body.appendChild(frame);

          frame.addEventListener("load", function() {
            ok(false, "load should not finish before the frame is removed");
          });
        }
      );

      is(
        XULBrowserWindow.isBusy,
        true,
        "browser should be busy after the load starts"
      );

      // Remove the outer iframe, ending the load within this frame's subframe
      // early.
      await SpecialPowers.spawn(browser, [], async () => {
        content.document.querySelector("iframe").remove();
      });

      await TestUtils.waitForCondition(
        () => !XULBrowserWindow.isBusy,
        "Browser should no longer be busy after the frame is removed"
      );
    }
  );
});
