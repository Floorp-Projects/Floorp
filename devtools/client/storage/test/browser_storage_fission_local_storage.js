/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function() {
  const URL_IFRAME = buildURLWithContent(
    "example.net",
    `<h1>iframe</h1>` +
      `<script>localStorage.setItem("lorem", "ipsum");</script>`
  );
  const URL_MAIN = buildURLWithContent(
    "example.com",
    `<h1>Main</h1>` +
      `<script>localStorage.setItem("foo", "bar");</script>` +
      `<iframe src="${URL_IFRAME}">`
  );

  // open tab
  await openTabAndSetupStorage(URL_MAIN);
  const doc = gPanelWindow.document;

  // check that both hosts appear in the storage tree
  checkTree(doc, ["localStorage", "http://example.com"]);
  // check the table for values
  await selectTreeItem(["localStorage", "http://example.com"]);
  await waitForStorageData("foo", "bar");
  await selectTreeItem(["localStorage", "http://example.net"]);
  await waitForStorageData("lorem", "ipsum");

  // add more storage data to the main wrapper
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    content.window.localStorage.setItem("foo2", "bar2");
    const iframe = content.document.querySelector("iframe");
    return SpecialPowers.spawn(iframe, [], () => {
      content.window.localStorage.setItem("lorem2", "ipsum2");
    });
  });
  // check that the new data is shown in the table
  await selectTreeItem(["localStorage", "http://example.com"]);
  await waitForStorageData("foo2", "bar2");
  await selectTreeItem(["localStorage", "http://example.net"]);
  await waitForStorageData("lorem2", "ipsum2");
});
