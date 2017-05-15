add_task(function* () {
  let tabs = [ gBrowser.selectedTab, BrowserTestUtils.addTab(gBrowser) ];

  // The first tab has an autofocused element.
  // The second tab is exactly like the first one without the autofocus.
  let testingList = [
    { uri: "data:text/html,<!DOCTYPE html><html><body><input autofocus id='target'></body></html>",
      tagName: "INPUT"},
  ];

  // Set the focus to the first tab.
  tabs[0].linkedBrowser.focus();

  // Load the second tab in the background.
  let loadedPromise = BrowserTestUtils.browserLoaded(tabs[1].linkedBrowser);
  tabs[1].linkedBrowser.loadURI(testingList[0].uri);
  yield loadedPromise;

  for (var i = 0; i < testingList.length; ++i) {
    // Get active element in the tab.
    let tagName = yield ContentTask.spawn(tabs[i + 1].linkedBrowser, null, function* () {
      return content.document.activeElement.tagName;
    });

    is(tagName, testingList[i].tagName,
       "The background tab's focused element should be " + testingList[i].tagName);
  }

  is(document.activeElement, tabs[0].linkedBrowser,
     "The background tab's focused element should not cause the tab to be focused");

  // Cleaning up.
  for (let i = 1; i < tabs.length; i++) {
    yield BrowserTestUtils.removeTab(tabs[i]);
  }
});

