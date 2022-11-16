/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
add_task(async function() {
  // Create a new content tab to make sure the paste is cross-process.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "data:text/html,<br>"
  );
  let browser = tab.linkedBrowser;

  await SpecialPowers.spawn(browser, [], async function(arg) {
    const trans = Cc["@mozilla.org/widget/transferable;1"].createInstance(
      Ci.nsITransferable
    );
    trans.init(null);

    const string = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString
    );
    string.data = "blablabla";

    trans.addDataFlavor("text/unknown");
    trans.setTransferData("text/unknown", string);

    trans.addDataFlavor("text/unicode");
    trans.setTransferData("text/unicode", string);

    // Write to clipboard.
    Services.clipboard.setData(trans, null, Ci.nsIClipboard.kGlobalClipboard);
  });

  // Wait for known.
  for (var i = 0; i < 20; i++) {
    if (
      Services.clipboard.hasDataMatchingFlavors(
        ["text/unicode"],
        Services.clipboard.kGlobalClipboard
      )
    ) {
      break;
    }
  }

  function readClipboard(flavor) {
    const trans = Cc["@mozilla.org/widget/transferable;1"].createInstance(
      Ci.nsITransferable
    );
    trans.init(null);
    trans.addDataFlavor(flavor);
    Services.clipboard.getData(trans, Services.clipboard.kGlobalClipboard);

    let data = {};
    trans.getTransferData(flavor, data);
    return data.value.QueryInterface(Ci.nsISupportsString).data;
  }

  ok(
    Services.clipboard.hasDataMatchingFlavors(
      ["text/unicode"],
      Services.clipboard.kGlobalClipboard
    ),
    "clipboard should have text/unicode"
  );

  is(
    readClipboard("text/unicode"),
    "blablabla",
    "matching string for text/unicode"
  );

  ok(
    !Services.clipboard.hasDataMatchingFlavors(
      ["text/unknown"],
      Services.clipboard.kGlobalClipboard
    ),
    "clipboard should not have text/unknown"
  );

  let error = undefined;
  try {
    readClipboard("text/unknown");
  } catch (e) {
    error = e;
  }
  is(typeof error, "object", "reading text/unknown should fail");

  BrowserTestUtils.removeTab(tab);
});
