/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This test makes sure that the style editor does not store any
// content CSS files in the permanent cache when opened from a private window tab.

const TEST_URL = `http://${TEST_HOST}/browser/devtools/client/styleeditor/test/test_private.html`;

add_task(async function() {
  info("Opening a new private window");
  const win = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  info("Clearing the browser cache");
  Services.cache2.clear();

  const { toolbox, ui } = await openStyleEditorForURL(TEST_URL, win);

  is(ui.editors.length, 1, "The style editor contains one sheet.");
  const editor = ui.editors[0];
  is(
    editor.friendlyName,
    "test_private.css",
    "The style editor contains the expected stylesheet"
  );

  await editor.getSourceEditor();

  await checkDiskCacheFor(editor.friendlyName);

  await toolbox.destroy();

  const onUnload = new Promise(done => {
    win.addEventListener("unload", function listener(event) {
      if (event.target == win.document) {
        win.removeEventListener("unload", listener);
        done();
      }
    });
  });
  win.close();
  await onUnload;
});

function checkDiskCacheFor(fileName) {
  let foundPrivateData = false;

  return new Promise(resolve => {
    Visitor.prototype = {
      onCacheStorageInfo(num) {
        info("disk storage contains " + num + " entries");
      },
      onCacheEntryInfo(uri) {
        const urispec = uri.asciiSpec;
        info(urispec);
        foundPrivateData = foundPrivateData || urispec.includes(fileName);
      },
      onCacheEntryVisitCompleted() {
        is(foundPrivateData, false, "web content present in disk cache");
        resolve();
      },
    };
    function Visitor() {}

    const storage = Services.cache2.diskCacheStorage(
      Services.loadContextInfo.default
    );
    storage.asyncVisitStorage(
      new Visitor(),
      /* Do walk entries */
      true
    );
  });
}
