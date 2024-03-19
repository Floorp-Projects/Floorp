/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that (real) files can be pasted into chrome/content.
// Pasting files should also hide all other data from content.

function setClipboard(path) {
  const file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(path);

  const trans = Cc["@mozilla.org/widget/transferable;1"].createInstance(
    Ci.nsITransferable
  );
  trans.init(null);
  trans.addDataFlavor("application/x-moz-file");
  trans.setTransferData("application/x-moz-file", file);

  trans.addDataFlavor("text/plain");
  const str = Cc["@mozilla.org/supports-string;1"].createInstance(
    Ci.nsISupportsString
  );
  str.data = "Alternate";
  trans.setTransferData("text/plain", str);

  // Write to clipboard.
  Services.clipboard.setData(trans, null, Ci.nsIClipboard.kGlobalClipboard);
}

add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.events.dataTransfer.mozFile.enabled", true]],
  });

  // Create a temporary file that will be pasted.
  const file = await IOUtils.createUniqueFile(
    PathUtils.tempDir,
    "test-file.txt",
    0o600
  );
  await IOUtils.writeUTF8(file, "Hello World!");

  // Put the data directly onto the native clipboard to make sure
  // it isn't handled internally in Gecko somehow.
  setClipboard(file);

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/browser/browser/base/content/test/general/clipboard_pastefile.html"
  );
  let browser = tab.linkedBrowser;

  let resultPromise = SpecialPowers.spawn(browser, [], function () {
    return new Promise(resolve => {
      content.document.addEventListener("testresult", event => {
        resolve(event.detail.result);
      });
    });
  });

  // Focus <input> in content
  await SpecialPowers.spawn(browser, [], async function () {
    content.document.getElementById("input").focus();
  });

  // Paste file into <input> in content
  await BrowserTestUtils.synthesizeKey("v", { accelKey: true }, browser);

  let result = await resultPromise;
  is(result, PathUtils.filename(file), "Correctly pasted file in content");

  var input = document.createElement("input");
  document.documentElement.appendChild(input);
  input.focus();

  await new Promise(resolve => {
    input.addEventListener(
      "paste",
      function (event) {
        let dt = event.clipboardData;
        is(dt.types.length, 3, "number of types");
        ok(dt.types.includes("text/plain"), "text/plain exists in types");
        ok(
          dt.types.includes("application/x-moz-file"),
          "application/x-moz-file exists in types"
        );
        is(dt.types[2], "Files", "Last type should be 'Files'");
        ok(
          dt.mozTypesAt(0).contains("text/plain"),
          "text/plain exists in mozTypesAt"
        );
        is(
          dt.getData("text/plain"),
          "Alternate",
          "text/plain returned in getData"
        );
        is(
          dt.mozGetDataAt("text/plain", 0),
          "Alternate",
          "text/plain returned in mozGetDataAt"
        );

        ok(
          dt.mozTypesAt(0).contains("application/x-moz-file"),
          "application/x-moz-file exists in mozTypesAt"
        );
        let mozFile = dt.mozGetDataAt("application/x-moz-file", 0);

        ok(
          mozFile instanceof Ci.nsIFile,
          "application/x-moz-file returned nsIFile with mozGetDataAt"
        );

        is(
          mozFile.leafName,
          PathUtils.filename(file),
          "nsIFile has correct leafName"
        );

        resolve();
      },
      { capture: true, once: true }
    );

    EventUtils.synthesizeKey("v", { accelKey: true });
  });

  input.remove();

  BrowserTestUtils.removeTab(tab);

  await IOUtils.remove(file);
});
