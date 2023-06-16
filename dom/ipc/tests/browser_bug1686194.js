/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_PAGE =
  "http://mochi.test:8888/browser/dom/ipc/tests/file_dummy.html";

function untilPageTitleChanged() {
  return new Promise(resolve =>
    gBrowser.addEventListener("pagetitlechanged", resolve, { once: true })
  );
}

add_task(async () => {
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_PAGE,
  });

  const { linkedBrowser } = tab;
  ok(
    tab.getAttribute("label").includes("file_dummy.html"),
    "The title should be the raw path"
  );

  await Promise.all([
    SpecialPowers.spawn(linkedBrowser, [], function () {
      content.document.title = "Title";
    }),
    untilPageTitleChanged(),
  ]);

  is(tab.getAttribute("label"), "Title", "The title should change");

  linkedBrowser.reload();

  await untilPageTitleChanged();

  ok(
    tab.getAttribute("label").includes("file_dummy.html"),
    "The title should be the raw path again"
  );

  BrowserTestUtils.removeTab(tab);
});
