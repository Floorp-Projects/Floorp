/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head.js */

"use strict";

add_task(async function() {
  const URL_IFRAME = buildURLWithContent(
    "example.net",
    `<h1>iframe</h1>` + `<script>document.cookie = "lorem=ipsum";</script>`
  );

  const URL_MAIN = buildURLWithContent(
    "example.com",
    `<h1>Main</h1>` +
      `<script>document.cookie="foo=bar";</script>` +
      `<iframe src="${URL_IFRAME}">`
  );

  // open tab
  await openTabAndSetupStorage(URL_MAIN);
  const doc = gPanelWindow.document;

  // check that both hosts appear in the storage tree
  checkTree(doc, ["cookies", "http://example.com"]);
  checkTree(doc, ["cookies", "http://example.net"]);
  // check the table for values
  await selectTreeItem(["cookies", "http://example.com"]);
  checkCookieData("foo", "bar");
  await selectTreeItem(["cookies", "http://example.net"]);
  checkCookieData("lorem", "ipsum");

  info("Add more cookies");
  const onUpdated = gUI.once("store-objects-edit");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    content.window.document.cookie = "foo2=bar2";

    const iframe = content.document.querySelector("iframe");
    return SpecialPowers.spawn(iframe, [], () => {
      content.document.cookie = "lorem2=ipsum2";
    });
  });
  await onUpdated;

  // check that the new data is shown in the table for the iframe document
  checkCookieData("lorem2", "ipsum2");

  // check that the new data is shown in the table for the top-level document
  await selectTreeItem(["cookies", "http://example.com"]);
  checkCookieData("foo2", "bar2");
});
