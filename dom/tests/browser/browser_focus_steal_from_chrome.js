function test() {
  waitForExplicitFinish();

  let secMan = Components.classes["@mozilla.org/scriptsecuritymanager;1"]
                         .getService(Components.interfaces
                                               .nsIScriptSecurityManager);

  let fm = Components.classes["@mozilla.org/focus-manager;1"]
                     .getService(Components.interfaces.nsIFocusManager);

  let tabs = [ gBrowser.selectedTab, gBrowser.addTab() ];

  let testingList = [
    { uri: "data:text/html,<body onload=\"setTimeout(function () { document.getElementById('target').focus(); }, 10);\"><input id='target'></body>",
      tagName: "INPUT", methodName: "focus" },
    { uri: "data:text/html,<body onload=\"setTimeout(function () { document.getElementById('target').select(); }, 10);\"><input id='target'></body>",
      tagName: "INPUT", methodName: "select" },
    { uri: "data:text/html,<body onload=\"setTimeout(function () { document.getElementById('target').focus(); }, 10);\"><a href='about:blank' id='target'>anchor</a></body>",
      tagName: "A", methodName: "focus" },
    { uri: "data:text/html,<body onload=\"setTimeout(function () { document.getElementById('target').focus(); }, 10);\"><button id='target'>button</button></body>",
      tagName: "BUTTON", methodName: "focus" },
    { uri: "data:text/html,<body onload=\"setTimeout(function () { document.getElementById('target').focus(); }, 10);\"><select id='target'><option>item1</option></select></body>",
      tagName: "SELECT", methodName: "focus" },
    { uri: "data:text/html,<body onload=\"setTimeout(function () { document.getElementById('target').focus(); }, 10);\"><textarea id='target'>textarea</textarea></body>",
      tagName: "TEXTAREA", methodName: "focus" },
    { uri: "data:text/html,<body onload=\"setTimeout(function () { document.getElementById('target').select(); }, 10);\"><textarea id='target'>textarea</textarea></body>",
      tagName: "TEXTAREA", methodName: "select" },
    { uri: "data:text/html,<body onload=\"setTimeout(function () { document.getElementById('target').focus(); }, 10);\"><label id='target'><input></label></body>",
      tagName: "INPUT", methodName: "focus of label element" },
    { uri: "data:text/html,<body onload=\"setTimeout(function () { document.getElementById('target').focus(); }, 10);\"><fieldset><legend id='target'>legend</legend><input></fieldset></body>",
      tagName: "INPUT", methodName: "focus of legend element" },
    { uri: "data:text/html,<body onload=\"setTimeout(function () {" +
           "  var element = document.getElementById('target');" +
           "  var event = document.createEvent('MouseEvent');" +
           "  event.initMouseEvent('mousedown', true, true, window," +
           "    0, 0, 0, 0, 0, false, false, false, false, 0, element);" +
           "  element.dispatchEvent(event); }, 10);\">" +
           "<button id='target'>button</button></body>",
      tagName: "BUTTON", methodName: "mousedown event on the button element" },
    { uri: "data:text/html,<body onload=\"setTimeout(function () {" +
           "  var element = document.getElementById('target');" +
           "  var event = document.createEvent('MouseEvent');" +
           "  event.initMouseEvent('click', true, true, window," +
           "    1, 0, 0, 0, 0, false, false, false, false, 0, element);" +
           "  element.dispatchEvent(event); }, 10);\">" +
           "<label id='target'><input></label></body>",
      tagName: "INPUT", methodName: "click event on the label element" },
  ];

  let testingIndex = -1;
  let canRetry;
  let callback;
  let loadedCount;

  function runNextTest() {
    if (++testingIndex >= testingList.length) {
      // cleaning-up...
      gBrowser.addTab();
      for (let i = 0; i < tabs.length; i++) {
        gBrowser.removeTab(tabs[i]);
      }
      finish();
      return;
    }
    callback = doTest1;
    loadTestPage();
  }

  function loadTestPage() {
    loadedCount = 0;
    canRetry = 10;
    // Set the focus to the contents.
    tabs[0].linkedBrowser.focus();
    for (let i = 0; i < tabs.length; i++) {
      tabs[i].linkedBrowser.addEventListener("load", onLoad, true);
      tabs[i].linkedBrowser.loadURI(testingList[testingIndex].uri);
    }
  }

  function onLoad() {
    if (++loadedCount == tabs.length) {
      for (let i = 0; i < tabs.length; i++) {
        tabs[i].linkedBrowser.removeEventListener("load", onLoad, true);
      }
      setTimeout(callback, 20);
    }
  }

  function isPrepared() {
    for (let i = 0; i < tabs.length; i++) {
      if (tabs[i].linkedBrowser.contentDocument.activeElement.tagName !=
            testingList[testingIndex].tagName) {
        return false;
      }
    }
    return true;
  }

  function doTest1() {
    if (canRetry-- > 0 && !isPrepared()) {
      setTimeout(callback, 10); // retry
      return;
    }

    // The contents should be able to steal the focus from content.

    // in foreground tab
    let e = tabs[0].linkedBrowser.contentDocument.activeElement;
    is(e.tagName, testingList[testingIndex].tagName,
       "the foreground tab's " + testingList[testingIndex].tagName +
       " element is not active by the " + testingList[testingIndex].methodName +
       " (Test1: content can steal focus)");
    is(fm.focusedElement, e,
       "the " + testingList[testingIndex].tagName +
       " element isn't focused by the " + testingList[testingIndex].methodName +
       " (Test1: content can steal focus)");

    // in background tab
    e = tabs[1].linkedBrowser.contentDocument.activeElement;
    is(e.tagName, testingList[testingIndex].tagName,
       "the background tab's " + testingList[testingIndex].tagName +
       " element is not active by the " + testingList[testingIndex].methodName +
       " (Test1: content can steal focus)");
    isnot(fm.focusedElement, e,
          "the " + testingList[testingIndex].tagName +
          " element is focused by the " + testingList[testingIndex].methodName +
          " (Test1: content can steal focus)");

    callback = doTest2;
    loadTestPage();

    // Set focus to chrome element before onload events of the loading contents.
    BrowserSearch.searchBar.focus();
  }


  function doTest2() {
    if (canRetry-- > 0 && !isPrepared()) {
      setTimeout(callback, 10); // retry
      return;
    }

    // XXX puts some useful information for bug 534420
    let activeWindow = fm.activeWindow;
    let focusedWindow = fm.focusedWindow;
    ok(activeWindow, "We're not active");
    ok(focusedWindow, "There is no focused window");
    is(activeWindow, window.top, "our window isn't active");
    let searchbar = BrowserSearch.searchBar;
    let focusedElement = fm.focusedElement;
    if (searchbar) {
      let principal = searchbar.nodePrincipal;
      ok(principal, "principal is null");
      info("search bar: tagName=" + searchbar.tagName + " id=" + searchbar.id);
      ok(secMan.isSystemPrincipal(principal), "search bar isn't chrome");
    } else {
      info("search bar is NULL!!");
    }
    if (focusedElement) {
      let principal = focusedElement.nodePrincipal;
      ok(principal, "principal is null");
      info("focusedElement: tagName=" + focusedElement.tagName +
           " id=" + focusedElement.id);
      ok(secMan.isSystemPrincipal(principal), "focusedElement isn't chrome");
    } else {
      info("focusedElement is NULL!!");
    }

    // The contents shouldn't be able to steal the focus from chrome.

    // in foreground tab
    let e = tabs[0].linkedBrowser.contentDocument.activeElement;
    is(e.tagName, testingList[testingIndex].tagName,
       "the foreground tab's " + testingList[testingIndex].tagName +
       " element is not active by the " + testingList[testingIndex].methodName +
       " (Test2: content can NOT steal focus)");
    isnot(fm.focusedElement, e,
          "the " + testingList[testingIndex].tagName +
          " element is focused by the " + testingList[testingIndex].methodName +
          " (Test2: content can NOT steal focus)");

    // in background tab
    e = tabs[1].linkedBrowser.contentDocument.activeElement;
    is(e.tagName, testingList[testingIndex].tagName,
       "the background tab's " + testingList[testingIndex].tagName +
       " element is not active by the " + testingList[testingIndex].methodName +
       " (Test2: content can NOT steal focus)");
    isnot(fm.focusedElement, e,
          "the " + testingList[testingIndex].tagName +
          " element is focused by the " + testingList[testingIndex].methodName +
          " (Test2: content can NOT steal focus)");

    runNextTest();
  }

  runNextTest();
}
