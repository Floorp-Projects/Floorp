/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function () {
  const html = `<h1>about:blank iframe</h1><iframe src="about:blank"></iframe>`;
  const url = `https://example.com/document-builder.sjs?html=${encodeURI(
    html
  )}`;
  // open tab
  await openTabAndSetupStorage(url);
  const doc = gPanelWindow.document;

  checkTree(doc, ["localStorage", "https://example.com"], true);
  checkTree(doc, ["localStorage", "about:blank"], false);
});

add_task(async function () {
  // open tab with about:blank as top-level page
  await openTabAndSetupStorage("about:blank");
  const doc = gPanelWindow.document;

  checkTree(doc, ["localStorage"], true);
});
