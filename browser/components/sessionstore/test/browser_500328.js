/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function checkState(tab) {
  // Go back and then forward, and make sure that the state objects received
  // from the popState event are as we expect them to be.
  //
  // We also add a node to the document's body when after going back and make
  // sure it's still there after we go forward -- this is to test that the two
  // history entries correspond to the same document.

  let popStateCount = 0;

  tab.linkedBrowser.addEventListener('popstate', function(aEvent) {
    let contentWindow = tab.linkedBrowser.contentWindow;
    if (popStateCount == 0) {
      popStateCount++;

      is(tab.linkedBrowser.contentWindow.testState, 'foo',
         'testState after going back');

      ok(aEvent.state, "Event should have a state property.");
      is(JSON.stringify(tab.linkedBrowser.contentWindow.history.state), JSON.stringify({obj1:1}),
         "first popstate object.");

      // Add a node with id "new-elem" to the document.
      let doc = contentWindow.document;
      ok(!doc.getElementById("new-elem"),
         "doc shouldn't contain new-elem before we add it.");
      let elem = doc.createElement("div");
      elem.id = "new-elem";
      doc.body.appendChild(elem);

      tab.linkedBrowser.goForward();
    }
    else if (popStateCount == 1) {
      popStateCount++;
      // When content fires a PopStateEvent and we observe it from a chrome event
      // listener (as we do here, and, thankfully, nowhere else in the tree), the
      // state object will be a cross-compartment wrapper to an object that was
      // deserialized in the content scope. And in this case, since RegExps are
      // not currently Xrayable (see bug 1014991), trying to pull |obj3| (a RegExp)
      // off of an Xrayed Object won't work. So we need to waive.
      runInContent(tab.linkedBrowser, function(win, state) {
        return Cu.waiveXrays(state).obj3.toString();
      }, aEvent.state).then(function(stateStr) {
        is(stateStr, '/^a$/', "second popstate object.");

        // Make sure that the new-elem node is present in the document.  If it's
        // not, then this history entry has a different doc identifier than the
        // previous entry, which is bad.
        let doc = contentWindow.document;
        let newElem = doc.getElementById("new-elem");
        ok(newElem, "doc should contain new-elem.");
        newElem.parentNode.removeChild(newElem);
        ok(!doc.getElementById("new-elem"), "new-elem should be removed.");

        tab.linkedBrowser.removeEventListener("popstate", arguments.callee, true);
        gBrowser.removeTab(tab);
        finish();
      });
    }
  });

  // Set some state in the page's window.  When we go back(), the page should
  // be retrieved from bfcache, and this state should still be there.
  tab.linkedBrowser.contentWindow.testState = 'foo';

  // Now go back.  This should trigger the popstate event handler above.
  tab.linkedBrowser.goBack();
}

function test() {
  // Tests session restore functionality of history.pushState and
  // history.replaceState().  (Bug 500328)

  waitForExplicitFinish();

  // We open a new blank window, let it load, and then load in
  // http://example.com.  We need to load the blank window first, otherwise the
  // docshell gets confused and doesn't have a current history entry.
  let tab = gBrowser.addTab("about:blank");
  let browser = tab.linkedBrowser;

  promiseBrowserLoaded(browser).then(() => {
    browser.loadURI("http://example.com", null, null);

    promiseBrowserLoaded(browser).then(() => {
      // After these push/replaceState calls, the window should have three
      // history entries:
      //   testURL        (state object: null)          <-- oldest
      //   testURL        (state object: {obj1:1})
      //   testURL?page2  (state object: {obj3:/^a$/})  <-- newest
      function contentTest(win) {
        let history = win.history;
        history.pushState({obj1:1}, "title-obj1");
        history.pushState({obj2:2}, "title-obj2", "?page2");
        history.replaceState({obj3:/^a$/}, "title-obj3");
      }
      runInContent(browser, contentTest, null).then(function() {
        TabState.flush(tab.linkedBrowser);
        let state = ss.getTabState(tab);
        gBrowser.removeTab(tab);

        // Restore the state into a new tab.  Things don't work well when we
        // restore into the old tab, but that's not a real use case anyway.
        let tab2 = gBrowser.addTab("about:blank");
        ss.setTabState(tab2, state, true);

        // Run checkState() once the tab finishes loading its restored state.
        promiseTabRestored(tab2).then(() => checkState(tab2));
      });
    });
  });
}
