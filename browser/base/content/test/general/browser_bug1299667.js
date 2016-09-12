/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { addObserver, removeObserver } = Cc["@mozilla.org/observer-service;1"].
                                          getService(Ci.nsIObserverService);

function receive(topic) {
  return new Promise((resolve, reject) => {
    let timeout = setTimeout(() => {
      reject(new Error("Timeout"));
    }, 90000);

    const observer = {
      observe: subject => {
        removeObserver(observer, topic);
        clearTimeout(timeout);
        resolve(subject);
      }
    };
    addObserver(observer, topic, false);
  });
}

add_task(function* () {
  yield BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com");

  yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    content.history.pushState({}, "2", "2.html");
  });

  yield receive("sessionstore-state-write-complete");

  // Wait for the session data to be flushed before continuing the test
  yield new Promise(resolve => SessionStore.getSessionHistory(gBrowser.selectedTab, resolve));

  var backButton = document.getElementById("back-button");
  var contextMenu = document.getElementById("backForwardMenu");
  var rect = backButton.getBoundingClientRect();

  info("waiting for the history menu to open");

  let popupShownPromise = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(backButton, {type: "contextmenu", button: 2});
  let event = yield popupShownPromise;

  ok(true, "history menu opened");

  // Wait for the session data to be flushed before continuing the test
  yield new Promise(resolve => SessionStore.getSessionHistory(gBrowser.selectedTab, resolve));

  is(event.target.children.length, 2, "Two history items");

  let node = event.target.firstChild;
  is(node.getAttribute("uri"), "http://example.com/2.html", "first item uri");
  is(node.getAttribute("index"), "1", "first item index");
  is(node.getAttribute("historyindex"), "0", "first item historyindex");

  node = event.target.lastChild;
  is(node.getAttribute("uri"), "http://example.com/", "second item uri");
  is(node.getAttribute("index"), "0", "second item index");
  is(node.getAttribute("historyindex"), "-1", "second item historyindex");

  let popupHiddenPromise = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
  event.target.hidePopup();
  yield popupHiddenPromise;
  info("Hidden popup");

  let onClose = BrowserTestUtils.waitForEvent(gBrowser.tabContainer, "TabClose");
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
  yield onClose;
  info("Tab closed");
});
