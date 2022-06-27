/*
 * This test checks that focus is adjusted properly when switching tabs.
 */

/* eslint-env mozilla/frame-script */

var testPage1 =
  "<html id='html1'><body id='body1'><button id='button1'>Tab 1</button></body></html>";
var testPage2 =
  "<html id='html2'><body id='body2'><button id='button2'>Tab 2</button></body></html>";
var testPage3 =
  "<html id='html3'><body id='body3'><button id='button3'>Tab 3</button></body></html>";

const fm = Services.focus;

function EventStore() {
  this["main-window"] = [];
  this.window1 = [];
  this.window2 = [];
}

EventStore.prototype = {
  push(event) {
    if (event.includes("browser1") || event.includes("browser2")) {
      this["main-window"].push(event);
    } else if (event.includes("1")) {
      this.window1.push(event);
    } else if (event.includes("2")) {
      this.window2.push(event);
    } else {
      this["main-window"].push(event);
    }
  },
};

var tab1 = null;
var tab2 = null;
var browser1 = null;
var browser2 = null;
var _lastfocus;
var _lastfocuswindow = null;
var actualEvents = new EventStore();
var expectedEvents = new EventStore();
var currentTestName = "";
var _expectedElement = null;
var _expectedWindow = null;

var currentPromiseResolver = null;

function getFocusedElementForBrowser(browser, dontCheckExtraFocus = false) {
  return SpecialPowers.spawn(
    browser,
    [dontCheckExtraFocus],
    dontCheckExtraFocusChild => {
      const { Services } = ChromeUtils.import(
        "resource://gre/modules/Services.jsm"
      );

      let focusedWindow = {};
      let node = Services.focus.getFocusedElementForWindow(
        content,
        false,
        focusedWindow
      );
      let details = "Focus is " + (node ? node.id : "<none>");

      /* Check focus manager properties. Add an error onto the string if they are
       not what is expected which will cause matching to fail in the parent process. */
      let doc = content.document;
      if (!dontCheckExtraFocusChild) {
        if (Services.focus.focusedElement != node) {
          details += "<ERROR: focusedElement doesn't match>";
        }
        if (
          Services.focus.focusedWindow &&
          Services.focus.focusedWindow != content
        ) {
          details += "<ERROR: focusedWindow doesn't match>";
        }
        if ((Services.focus.focusedWindow == content) != doc.hasFocus()) {
          details += "<ERROR: child hasFocus() is not correct>";
        }
        if (
          (Services.focus.focusedElement &&
            doc.activeElement != Services.focus.focusedElement) ||
          (!Services.focus.focusedElement && doc.activeElement != doc.body)
        ) {
          details += "<ERROR: child activeElement is not correct>";
        }
      }
      return details;
    }
  );
}

function focusInChild(event) {
  function getWindowDocId(target) {
    return String(target.location).includes("1") ? "window1" : "window2";
  }

  // Stop the shim code from seeing this event process.
  event.stopImmediatePropagation();

  var id;
  if (event.target instanceof Ci.nsIDOMWindow) {
    id = getWindowDocId(event.originalTarget) + "-window";
  } else if (event.target.nodeType == event.target.DOCUMENT_NODE) {
    id = getWindowDocId(event.originalTarget) + "-document";
  } else {
    id = event.originalTarget.id;
  }

  let window = event.target.ownerGlobal;
  if (!window._eventsOccurred) {
    window._eventsOccurred = [];
  }
  window._eventsOccurred.push(event.type + ": " + id);
  return true;
}

function focusElementInChild(elementid, elementtype) {
  let browser = elementid.includes("1") ? browser1 : browser2;
  return SpecialPowers.spawn(browser, [elementid, elementtype], (id, type) => {
    content.document.getElementById(id)[type]();
  });
}

add_task(async function() {
  tab1 = BrowserTestUtils.addTab(gBrowser);
  browser1 = gBrowser.getBrowserForTab(tab1);

  tab2 = BrowserTestUtils.addTab(gBrowser);
  browser2 = gBrowser.getBrowserForTab(tab2);

  await promiseTabLoadEvent(tab1, "data:text/html," + escape(testPage1));
  await promiseTabLoadEvent(tab2, "data:text/html," + escape(testPage2));

  gURLBar.focus();
  await SimpleTest.promiseFocus();

  // In these listeners, focusInChild is used to cache details about the event
  // on a temporary on the window (window._eventsOccurred), so that it can be
  // retrieved later within compareFocusResults. focusInChild always returns true.
  // compareFocusResults is called each time event occurs to check that the
  // right events happened.
  let listenersToRemove = [];
  listenersToRemove.push(
    BrowserTestUtils.addContentEventListener(
      browser1,
      "focus",
      compareFocusResults,
      { capture: true },
      focusInChild
    )
  );
  listenersToRemove.push(
    BrowserTestUtils.addContentEventListener(
      browser1,
      "blur",
      compareFocusResults,
      { capture: true },
      focusInChild
    )
  );
  listenersToRemove.push(
    BrowserTestUtils.addContentEventListener(
      browser2,
      "focus",
      compareFocusResults,
      { capture: true },
      focusInChild
    )
  );
  listenersToRemove.push(
    BrowserTestUtils.addContentEventListener(
      browser2,
      "blur",
      compareFocusResults,
      { capture: true },
      focusInChild
    )
  );

  // Get the content processes to do something, so that we can better
  // ensure that the listeners added above will have actually been added
  // in the tabs.
  await SpecialPowers.spawn(browser1, [], () => {});
  await SpecialPowers.spawn(browser2, [], () => {});

  _lastfocus = "urlbar";
  _lastfocuswindow = "main-window";

  window.addEventListener("focus", _browser_tabfocus_test_eventOccured, true);
  window.addEventListener("blur", _browser_tabfocus_test_eventOccured, true);

  // make sure that the focus initially starts out blank
  var focusedWindow = {};

  let focused = await getFocusedElementForBrowser(browser1);
  is(focused, "Focus is <none>", "initial focus in tab 1");

  focused = await getFocusedElementForBrowser(browser2);
  is(focused, "Focus is <none>", "initial focus in tab 2");

  is(
    document.activeElement,
    gURLBar.inputField,
    "focus after loading two tabs"
  );

  await expectFocusShiftAfterTabSwitch(
    tab2,
    "window2",
    null,
    true,
    "after tab change, focus in new tab"
  );

  focused = await getFocusedElementForBrowser(browser2);
  is(
    focused,
    "Focus is <none>",
    "focusedElement after tab change, focus in new tab"
  );

  // switching tabs when nothing in the new tab is focused
  // should focus the browser
  await expectFocusShiftAfterTabSwitch(
    tab1,
    "window1",
    null,
    true,
    "after tab change, focus in original tab"
  );

  focused = await getFocusedElementForBrowser(browser1);
  is(
    focused,
    "Focus is <none>",
    "focusedElement after tab change, focus in original tab"
  );

  // focusing a button in the current tab should focus it
  await expectFocusShift(
    () => focusElementInChild("button1", "focus"),
    "window1",
    "button1",
    true,
    "after button focused"
  );

  focused = await getFocusedElementForBrowser(browser1);
  is(
    focused,
    "Focus is button1",
    "focusedElement in first browser after button focused"
  );

  // focusing a button in a background tab should not change the actual
  // focus, but should set the focus that would be in that background tab to
  // that button.
  await expectFocusShift(
    () => focusElementInChild("button2", "focus"),
    "window1",
    "button1",
    false,
    "after button focus in unfocused tab"
  );

  focused = await getFocusedElementForBrowser(browser1, false);
  is(
    focused,
    "Focus is button1",
    "focusedElement in first browser after button focus in unfocused tab"
  );
  focused = await getFocusedElementForBrowser(browser2, true);
  is(
    focused,
    "Focus is button2",
    "focusedElement in second browser after button focus in unfocused tab"
  );

  // switching tabs should now make the button in the other tab focused
  await expectFocusShiftAfterTabSwitch(
    tab2,
    "window2",
    "button2",
    true,
    "after tab change with button focused"
  );

  // blurring an element in a background tab should not change the active
  // focus, but should clear the focus in that tab.
  await expectFocusShift(
    () => focusElementInChild("button1", "blur"),
    "window2",
    "button2",
    false,
    "focusedWindow after blur in unfocused tab"
  );

  focused = await getFocusedElementForBrowser(browser1, true);
  is(
    focused,
    "Focus is <none>",
    "focusedElement in first browser after focus in unfocused tab"
  );
  focused = await getFocusedElementForBrowser(browser2, false);
  is(
    focused,
    "Focus is button2",
    "focusedElement in second browser after focus in unfocused tab"
  );

  // When focus is in the tab bar, it should be retained there
  await expectFocusShift(
    () => gBrowser.selectedTab.focus(),
    "main-window",
    "tab2",
    true,
    "focusing tab element"
  );
  await expectFocusShiftAfterTabSwitch(
    tab1,
    "main-window",
    "tab1",
    true,
    "tab change when selected tab element was focused"
  );

  let switchWaiter = new Promise((resolve, reject) => {
    gBrowser.addEventListener(
      "TabSwitchDone",
      function() {
        executeSoon(resolve);
      },
      { once: true }
    );
  });

  await expectFocusShiftAfterTabSwitch(
    tab2,
    "main-window",
    "tab2",
    true,
    "another tab change when selected tab element was focused"
  );

  // Wait for the paint on the second browser so that any post tab-switching
  // stuff has time to complete before blurring the tab. Otherwise, the
  // _adjustFocusAfterTabSwitch in tabbrowser gets confused and isn't sure
  // what tab is really focused.
  await switchWaiter;

  await expectFocusShift(
    () => gBrowser.selectedTab.blur(),
    "main-window",
    null,
    true,
    "blurring tab element"
  );

  // focusing the url field should switch active focus away from the browser but
  // not clear what would be the focus in the browser
  await focusElementInChild("button1", "focus");

  await expectFocusShift(
    () => gURLBar.focus(),
    "main-window",
    "urlbar",
    true,
    "focusedWindow after url field focused"
  );
  focused = await getFocusedElementForBrowser(browser1, true);
  is(
    focused,
    "Focus is button1",
    "focusedElement after url field focused, first browser"
  );
  focused = await getFocusedElementForBrowser(browser2, true);
  is(
    focused,
    "Focus is button2",
    "focusedElement after url field focused, second browser"
  );

  await expectFocusShift(
    () => gURLBar.blur(),
    "main-window",
    null,
    true,
    "blurring url field"
  );

  // when a chrome element is focused, switching tabs to a tab with a button
  // with the current focus should focus the button
  await expectFocusShiftAfterTabSwitch(
    tab1,
    "window1",
    "button1",
    true,
    "after tab change, focus in url field, button focused in new tab"
  );

  focused = await getFocusedElementForBrowser(browser1, false);
  is(
    focused,
    "Focus is button1",
    "after switch tab, focus in unfocused tab, first browser"
  );
  focused = await getFocusedElementForBrowser(browser2, true);
  is(
    focused,
    "Focus is button2",
    "after switch tab, focus in unfocused tab, second browser"
  );

  // blurring an element in the current tab should clear the active focus
  await expectFocusShift(
    () => focusElementInChild("button1", "blur"),
    "window1",
    null,
    true,
    "after blur in focused tab"
  );

  focused = await getFocusedElementForBrowser(browser1, false);
  is(
    focused,
    "Focus is <none>",
    "focusedWindow after blur in focused tab, child"
  );
  focusedWindow = {};
  is(
    fm.getFocusedElementForWindow(window, false, focusedWindow),
    browser1,
    "focusedElement after blur in focused tab, parent"
  );

  // blurring an non-focused url field should have no effect
  await expectFocusShift(
    () => gURLBar.blur(),
    "window1",
    null,
    false,
    "after blur in unfocused url field"
  );

  focusedWindow = {};
  is(
    fm.getFocusedElementForWindow(window, false, focusedWindow),
    browser1,
    "focusedElement after blur in unfocused url field"
  );

  // switch focus to a tab with a currently focused element
  await expectFocusShiftAfterTabSwitch(
    tab2,
    "window2",
    "button2",
    true,
    "after switch from unfocused to focused tab"
  );
  focused = await getFocusedElementForBrowser(browser2, true);
  is(
    focused,
    "Focus is button2",
    "focusedElement after switch from unfocused to focused tab"
  );

  // clearing focus on the chrome window should switch the focus to the
  // chrome window
  await expectFocusShift(
    () => fm.clearFocus(window),
    "main-window",
    null,
    true,
    "after switch to chrome with no focused element"
  );

  focusedWindow = {};
  is(
    fm.getFocusedElementForWindow(window, false, focusedWindow),
    null,
    "focusedElement after switch to chrome with no focused element"
  );

  // switch focus to another tab when neither have an active focus
  await expectFocusShiftAfterTabSwitch(
    tab1,
    "window1",
    null,
    true,
    "focusedWindow after tab switch from no focus to no focus"
  );

  focused = await getFocusedElementForBrowser(browser1, false);
  is(
    focused,
    "Focus is <none>",
    "after tab switch from no focus to no focus, first browser"
  );
  focused = await getFocusedElementForBrowser(browser2, true);
  is(
    focused,
    "Focus is button2",
    "after tab switch from no focus to no focus, second browser"
  );

  // next, check whether navigating forward, focusing the urlbar and then
  // navigating back maintains the focus in the urlbar.
  await expectFocusShift(
    () => focusElementInChild("button1", "focus"),
    "window1",
    "button1",
    true,
    "focus button"
  );

  await promiseTabLoadEvent(tab1, "data:text/html," + escape(testPage3));

  // now go back again
  gURLBar.focus();

  await new Promise((resolve, reject) => {
    BrowserTestUtils.waitForContentEvent(
      window.gBrowser.selectedBrowser,
      "pageshow",
      true
    ).then(() => resolve());
    document.getElementById("Browser:Back").doCommand();
  });

  is(
    window.document.activeElement,
    gURLBar.inputField,
    "urlbar still focused after navigating back"
  );

  for (let listener of listenersToRemove) {
    listener();
  }

  window.removeEventListener(
    "focus",
    _browser_tabfocus_test_eventOccured,
    true
  );
  window.removeEventListener("blur", _browser_tabfocus_test_eventOccured, true);

  gBrowser.removeCurrentTab();
  gBrowser.removeCurrentTab();

  finish();
});

function _browser_tabfocus_test_eventOccured(event) {
  function getWindowDocId(target) {
    if (
      target == browser1.contentWindow ||
      target == browser1.contentDocument
    ) {
      return "window1";
    }
    if (
      target == browser2.contentWindow ||
      target == browser2.contentDocument
    ) {
      return "window2";
    }
    return "main-window";
  }

  var id;

  if (Window.isInstance(event.target)) {
    id = getWindowDocId(event.originalTarget) + "-window";
  } else if (Document.isInstance(event.target)) {
    id = getWindowDocId(event.originalTarget) + "-document";
  } else if (
    event.target.id == "urlbar" &&
    event.originalTarget.localName == "input"
  ) {
    id = "urlbar";
  } else if (event.originalTarget.localName == "browser") {
    id = event.originalTarget == browser1 ? "browser1" : "browser2";
  } else if (event.originalTarget.localName == "tab") {
    id = event.originalTarget == tab1 ? "tab1" : "tab2";
  } else {
    id = event.originalTarget.id;
  }

  actualEvents.push(event.type + ": " + id);
  compareFocusResults();
}

function getId(element) {
  if (!element) {
    return null;
  }

  if (element.localName == "browser") {
    return element == browser1 ? "browser1" : "browser2";
  }

  if (element.localName == "tab") {
    return element == tab1 ? "tab1" : "tab2";
  }

  return element.localName == "input" ? "urlbar" : element.id;
}

async function compareFocusResults() {
  if (!currentPromiseResolver) {
    return;
  }

  // Get the events that occurred in each child browser and store them
  // in 'actualEvents'. This is a global so if different calls to
  // compareFocusResults occur together, whichever one happens to get
  // called first after pulling all the events from the child will
  // perform the matching.
  let events = await SpecialPowers.spawn(browser1, [], () => {
    let eventsOccurred = content._eventsOccurred;
    content._eventsOccurred = [];
    return eventsOccurred || [];
  });
  actualEvents.window1.push(...events);

  events = await SpecialPowers.spawn(browser2, [], () => {
    let eventsOccurred = content._eventsOccurred;
    content._eventsOccurred = [];
    return eventsOccurred || [];
  });
  actualEvents.window2.push(...events);

  // Another call to compareFocusResults may have happened in the meantime.
  // If currentPromiseResolver is null, then that call was successful so no
  // need to check the events again.
  if (!currentPromiseResolver) {
    return;
  }

  let winIds = ["main-window", "window1", "window2"];

  for (let winId of winIds) {
    if (actualEvents[winId].length < expectedEvents[winId].length) {
      return;
    }
  }

  for (let winId of winIds) {
    for (let e = 0; e < expectedEvents.length; e++) {
      is(
        actualEvents[winId][e],
        expectedEvents[winId][e],
        currentTestName + " events [event " + e + "]"
      );
    }
    actualEvents[winId] = [];
  }

  let matchWindow = window;
  is(_expectedWindow, "main-window", "main-window is always expected");
  if (_expectedWindow == "main-window") {
    // The browser window's body doesn't have an id set usually - set one now
    // so it can be used for id comparisons below.
    matchWindow.document.body.id = "main-window-body";
  }

  var focusedElement = fm.focusedElement;
  is(
    getId(focusedElement),
    _expectedElement,
    currentTestName + " focusedElement"
  );

  is(fm.focusedWindow, matchWindow, currentTestName + " focusedWindow");
  var focusedWindow = {};
  is(
    getId(fm.getFocusedElementForWindow(matchWindow, false, focusedWindow)),
    _expectedElement,
    currentTestName + " getFocusedElementForWindow"
  );
  is(
    focusedWindow.value,
    matchWindow,
    currentTestName + " getFocusedElementForWindow frame"
  );
  is(matchWindow.document.hasFocus(), true, currentTestName + " hasFocus");
  var expectedActive = _expectedElement;
  if (!expectedActive) {
    expectedActive = getId(matchWindow.document.body);
  }
  is(
    getId(matchWindow.document.activeElement),
    expectedActive,
    currentTestName + " activeElement"
  );

  currentPromiseResolver();
  currentPromiseResolver = null;
}

async function expectFocusShiftAfterTabSwitch(
  tab,
  expectedWindow,
  expectedElement,
  focusChanged,
  testid
) {
  let tabSwitchPromise = null;
  await expectFocusShift(
    () => {
      tabSwitchPromise = BrowserTestUtils.switchTab(gBrowser, tab);
    },
    expectedWindow,
    expectedElement,
    focusChanged,
    testid
  );
  await tabSwitchPromise;
}

async function expectFocusShift(
  callback,
  expectedWindow,
  expectedElement,
  focusChanged,
  testid
) {
  currentPromiseResolver = null;
  currentTestName = testid;

  expectedEvents = new EventStore();

  if (focusChanged) {
    _expectedElement = expectedElement;
    _expectedWindow = expectedWindow;

    // When the content is in a child process, the expected element in the chrome window
    // will always be the urlbar or a browser element.
    if (_expectedWindow == "window1") {
      _expectedElement = "browser1";
    } else if (_expectedWindow == "window2") {
      _expectedElement = "browser2";
    }
    _expectedWindow = "main-window";

    if (
      _lastfocuswindow != "main-window" &&
      _lastfocuswindow != expectedWindow
    ) {
      let browserid = _lastfocuswindow == "window1" ? "browser1" : "browser2";
      expectedEvents.push("blur: " + browserid);
    }

    var newElementIsFocused =
      expectedElement && !expectedElement.startsWith("html");
    if (
      newElementIsFocused &&
      _lastfocuswindow != "main-window" &&
      expectedWindow == "main-window"
    ) {
      // When switching from a child to a chrome element, the focus on the element will arrive first.
      expectedEvents.push("focus: " + expectedElement);
      newElementIsFocused = false;
    }

    if (_lastfocus && _lastfocus != _expectedElement) {
      expectedEvents.push("blur: " + _lastfocus);
    }

    if (_lastfocuswindow && _lastfocuswindow != expectedWindow) {
      if (_lastfocuswindow != "main-window") {
        expectedEvents.push("blur: " + _lastfocuswindow + "-document");
        expectedEvents.push("blur: " + _lastfocuswindow + "-window");
      }
    }

    if (expectedWindow && _lastfocuswindow != expectedWindow) {
      if (expectedWindow != "main-window") {
        let browserid = expectedWindow == "window1" ? "browser1" : "browser2";
        expectedEvents.push("focus: " + browserid);
      }

      if (expectedWindow != "main-window") {
        expectedEvents.push("focus: " + expectedWindow + "-document");
        expectedEvents.push("focus: " + expectedWindow + "-window");
      }
    }

    if (newElementIsFocused) {
      expectedEvents.push("focus: " + expectedElement);
    }

    _lastfocus = expectedElement;
    _lastfocuswindow = expectedWindow;
  }

  // No events are expected, so return immediately. If events do occur, the following
  // tests will fail.
  if (
    expectedEvents["main-window"].length +
      expectedEvents.window1.length +
      expectedEvents.window2.length ==
    0
  ) {
    await callback();
    return undefined;
  }

  return new Promise(resolve => {
    currentPromiseResolver = resolve;
    callback();
  });
}
