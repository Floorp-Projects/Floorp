/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Tests bug 564900: Add folder specifically for downloads to Library left pane.
 * https://bugzilla.mozilla.org/show_bug.cgi?id=564900
 * This test visits various pages then opens the Library and ensures
 * that both the Downloads folder shows up and that the correct visits
 * are shown in it.
 */

add_task(async function test() {
  // Add visits.
  await PlacesTestUtils.addVisits([{
    uri: "http://mozilla.org",
    transition: PlacesUtils.history.TRANSITION_TYPED
  }, {
    uri: "http://google.com",
    transition: PlacesUtils.history.TRANSITION_DOWNLOAD
  }, {
    uri: "http://en.wikipedia.org",
    transition: PlacesUtils.history.TRANSITION_TYPED
  }, {
    uri: "http://ubuntu.org",
    transition: PlacesUtils.history.TRANSITION_DOWNLOAD
  }]);

  let library = await promiseLibrary("Downloads");

  registerCleanupFunction(async () => {
    await library.close();
    await PlacesUtils.history.clear();
  });

  // Make sure Downloads is present.
  Assert.notEqual(library.PlacesOrganizer._places.selectedNode, null,
    "Downloads is present and selected");

  // Check results.
  let testURIs = ["http://ubuntu.org/", "http://google.com/"];

  await BrowserTestUtils.waitForCondition(() =>
    library.ContentArea.currentView.associatedElement.children.length == testURIs.length);

  for (let element of library.ContentArea.currentView
                                          .associatedElement.children) {
    Assert.equal(element._shell.download.source.url, testURIs.shift(),
      "URI matches");
  }
});
