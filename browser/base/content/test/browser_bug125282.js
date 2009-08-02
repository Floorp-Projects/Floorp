function test() {
  waitForExplicitFinish();

  let fm = Components.classes["@mozilla.org/focus-manager;1"]
                     .getService(Components.interfaces.nsIFocusManager);

  let tabs = [ gBrowser.mCurrentTab, gBrowser.addTab() ];
  gBrowser.selectedTab = tabs[0];

  let loadedCount;

  // The first test set is checking the nsIDOMElement.focus can move focus at
  // onload event.
  let callback = doTest1;
  load("data:text/html,<body onload=\"document.getElementById('input').focus()\"><input id='input'></body>");

  function load(aURI) {
    loadedCount = 0;
    for (let i = 0; i < tabs.length; i++) {
      tabs[i].linkedBrowser.addEventListener("load", onLoad, true);
      tabs[i].linkedBrowser.loadURI(aURI);
    }
  }

  function onLoad() {
    if (++loadedCount == tabs.length) {
      setTimeout(callback, 10);
    }
  }

  function doTest1() {
    tabs[0].linkedBrowser.removeEventListener("load", onLoad, true);
    tabs[1].linkedBrowser.removeEventListener("load", onLoad, true);

    let e = tabs[0].linkedBrowser.contentDocument.activeElement;
    is(e.tagName, "INPUT", "the foreground tab's input element is not active");
    is(fm.focusedElement, e, "the input element isn't focused");
    e = tabs[1].linkedBrowser.contentDocument.activeElement;
    is(e.tagName, "INPUT", "the background tab's input element is not active");
    isnot(fm.focusedElement, e, "the input element is focused");

    // The second test set is checking the nsIDOMElement.focus can NOT move
    // if an element of chrome has focus.
    callback = doTest2;
    load("data:text/html,<body onload=\"setTimeout(function () { document.getElementById('input').focus(); }, 10);\"><input id='input'></body>");

    BrowserSearch.searchBar.focus();
  }

  let canRetry = 10;

  function doTest2() {
    if (canRetry-- > 0 &&
        (tabs[0].linkedBrowser.contentDocument.activeElement.tagName != "INPUT" ||
         tabs[1].linkedBrowser.contentDocument.activeElement.tagName != "INPUT")) {
      setTimeout(doTest2, 10); // retry
      return;
    }

    tabs[0].linkedBrowser.removeEventListener("load", onLoad, true);
    tabs[1].linkedBrowser.removeEventListener("load", onLoad, true);

    let e = tabs[0].linkedBrowser.contentDocument.activeElement;
    is(e.tagName, "INPUT", "the foreground tab's input element is not active");
    isnot(fm.focusedElement, e, "the input element is focused");
    e = tabs[1].linkedBrowser.contentDocument.activeElement;
    is(e.tagName, "INPUT", "the background tab's input element is not active");
    isnot(fm.focusedElement, e, "the input element is focused");

    // cleaning-up...
    tabs[0].linkedBrowser.loadURI("about:blank");
    gBrowser.selectedTab = tabs[1];
    gBrowser.removeCurrentTab();

    finish();
  }
}