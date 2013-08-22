// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function debugClipFlavors(aClip)
{
  let xfer = Cc["@mozilla.org/widget/transferable;1"].
             createInstance(Ci.nsITransferable);
  xfer.init(null);
  aClip.getData(xfer, Ci.nsIClipboard.kGlobalClipboard);
  let array = xfer.flavorsTransferableCanExport();
  let count = array.Count();
  info("flavors:" + count);
  for (let idx = 0; idx < count; idx++) {
    let string = array.GetElementAt(idx).QueryInterface(Ci.nsISupportsString);
    info("[" + idx + "] " + string);
  }
}

function checkContextMenuPositionRange(aElement, aMinLeft, aMaxLeft, aMinTop, aMaxTop) {
  ok(aElement.left > aMinLeft && aElement.left < aMaxLeft,
    "Left position is " + aElement.left + ", expected between " + aMinLeft + " and " + aMaxLeft);

  ok(aElement.top > aMinTop && aElement.top < aMaxTop,
    "Top position is " + aElement.top + ", expected between " + aMinTop + " and " + aMaxTop);
}

gTests.push({
  desc: "text context menu",
  run: function test() {
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

    info(chromeRoot + "browser_context_menu_tests_02.html");
    yield addTab(chromeRoot + "browser_context_menu_tests_02.html");

    purgeEventQueue();
    emptyClipboard();

    let win = Browser.selectedTab.browser.contentWindow;

    yield hideContextUI();

    ////////////////////////////////////////////////////////////
    // Context menu in content on selected text

    // select some text
    let span = win.document.getElementById("text1");
    win.getSelection().selectAllChildren(span);

    yield waitForMs(0);

    // invoke selection context menu
    let promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(win, span);
    yield promise;

    // should be visible
    ok(ContextMenuUI._menuPopup.visible, "is visible");

    // selected text context:
    checkContextUIMenuItemVisibility(["context-copy",
                                      "context-search"]);

    let menuItem = document.getElementById("context-copy");
    promise = waitForEvent(document, "popuphidden");
    EventUtils.synthesizeMouse(menuItem, 10, 10, {}, win);

    yield promise;

    // The wait is needed to give time to populate the clipboard.
    let string = "";
    yield waitForCondition(function () {
      string = SpecialPowers.getClipboardData("text/unicode");
      return string === span.textContent;
    });
    ok(string === span.textContent, "copied selected text from span");

    win.getSelection().removeAllRanges();

    ////////////////////////////////////////////////////////////
    // Context menu in content on selected text that includes a link

    // invoke selection with link context menu
    let link = win.document.getElementById("text2-link");
    win.getSelection().selectAllChildren(link);
    promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(win, link);
    yield promise;

    // should be visible
    ok(ContextMenuUI._menuPopup.visible, "is visible");

    // selected text context:
    checkContextUIMenuItemVisibility(["context-copy",
                                      "context-search",
                                      "context-open-in-new-tab",
                                      "context-copy-link"]);

    promise = waitForEvent(document, "popuphidden");
    ContextMenuUI.hide();
    yield promise;
    win.getSelection().removeAllRanges();

    ////////////////////////////////////////////////////////////
    // Context menu in content on a link

    link = win.document.getElementById("text2-link");
    promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(win, link);
    yield promise;

    // should be visible
    ok(ContextMenuUI._menuPopup.visible, "is visible");

    // selected text context:
    checkContextUIMenuItemVisibility(["context-open-in-new-tab",
                                      "context-copy-link",
                                      "context-bookmark-link"]);

    promise = waitForEvent(document, "popuphidden");
    ContextMenuUI.hide();
    yield promise;

    ////////////////////////////////////////////////////////////
    // context in input with no selection, no data on clipboard

    emptyClipboard();

    let input = win.document.getElementById("text3-input");
    promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(win, input);
    yield promise;

    // should be visible
    ok(ContextMenuUI._menuPopup.visible, "is visible");

    checkContextUIMenuItemVisibility(["context-select",
                                      "context-select-all"]);

    // copy menu item should not exist when no text is selected
    let menuItem = document.getElementById("context-copy");
    ok(menuItem && menuItem.hidden, "menu item is not visible");

    promise = waitForEvent(document, "popuphidden");
    ContextMenuUI.hide();
    yield promise;

    ////////////////////////////////////////////////////////////
    // context in input with selection copied to clipboard

    let input = win.document.getElementById("text3-input");
    input.value = "hello, I'm sorry but I must be going.";
    input.setSelectionRange(0, 5);
    promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(win, input, 20);
    yield promise;

    // should be visible
    ok(ContextMenuUI._menuPopup.visible, "is visible");

    checkContextUIMenuItemVisibility(["context-cut",
                                      "context-copy"]);

    let menuItem = document.getElementById("context-copy");
    let popupPromise = waitForEvent(document, "popuphidden");
    EventUtils.synthesizeMouse(menuItem, 10, 10, {}, win);

    yield popupPromise;

    // The wait is needed to give time to populate the clipboard.
    let string = "";
    yield waitForCondition(function () {
      string = SpecialPowers.getClipboardData("text/unicode");
      return string === "hello";
    });

    ok(string === "hello", "copied selected text");

    emptyClipboard();

    ////////////////////////////////////////////////////////////
    // context in input with text selection, no data on clipboard

    input = win.document.getElementById("text3-input");
    input.select();
    promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(win, input, 20);
    yield promise;

    // should be visible
    ok(ContextMenuUI._menuPopup.visible, "is visible");

    // selected text context:
    checkContextUIMenuItemVisibility(["context-cut",
                                      "context-copy"]);

    promise = waitForEvent(document, "popuphidden");
    ContextMenuUI.hide();
    yield promise;

    ////////////////////////////////////////////////////////////
    // context in input with no selection, data on clipboard

    SpecialPowers.clipboardCopyString("foo");
    input = win.document.getElementById("text3-input");
    input.select();
    promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(win, input, 20);
    yield promise;

    // should be visible
    ok(ContextMenuUI._menuPopup.visible, "is visible");

    // selected text context:
    checkContextUIMenuItemVisibility(["context-cut",
                                      "context-copy",
                                      "context-paste"]);

    promise = waitForEvent(document, "popuphidden");
    ContextMenuUI.hide();
    yield promise;

    ////////////////////////////////////////////////////////////
    // context in input with selection cut to clipboard

    emptyClipboard();

    let input = win.document.getElementById("text3-input");
    input.value = "hello, I'm sorry but I must be going.";
    input.setSelectionRange(0, 5);
    promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(win, input, 20);
    yield promise;

    // should be visible
    ok(ContextMenuUI._menuPopup.visible, "is visible");

    checkContextUIMenuItemVisibility(["context-cut",
                                      "context-copy"]);

    let menuItem = document.getElementById("context-cut");
    let popupPromise = waitForEvent(document, "popuphidden");
    EventUtils.synthesizeMouse(menuItem, 10, 10, {}, win);

    yield popupPromise;

    // The wait is needed to give time to populate the clipboard.
    let string = "";
    yield waitForCondition(function () {
      string = SpecialPowers.getClipboardData("text/unicode");
      return string === "hello";
    });

    let inputValue = input.value;
    ok(string === "hello", "cut selected text in clipboard");
    ok(inputValue === ", I'm sorry but I must be going.", "cut selected text from input value");

    emptyClipboard();

    ////////////////////////////////////////////////////////////
    // context in empty input, data on clipboard (paste operation)

    SpecialPowers.clipboardCopyString("foo");
    input = win.document.getElementById("text3-input");
    input.value = "";

    promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(win, input, 20);
    yield promise;

    // should be visible
    ok(ContextMenuUI._menuPopup.visible, "is visible");

    // selected text context:
    checkContextUIMenuItemVisibility(["context-paste"]);

    promise = waitForEvent(document, "popuphidden");
    ContextMenuUI.hide();
    yield promise;

    ////////////////////////////////////////////////////////////
    // context in empty input, no data on clipboard (??)

    emptyClipboard();
    ContextUI.dismiss();

    input = win.document.getElementById("text3-input");
    input.value = "";

    promise = waitForEvent(Elements.tray, "transitionend");
    sendContextMenuClickToElement(win, input, 20);
    yield promise;

    // should *not* be visible
    ok(!ContextMenuUI._menuPopup.visible, "is visible");

    // the test above will invoke the app bar
    yield hideContextUI();

    Browser.closeTab(Browser.selectedTab, { forceClose: true });
    purgeEventQueue();
  }
});

gTests.push({
  desc: "checks for context menu positioning when browser shifts",
  run: function test() {
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

    info(chromeRoot + "browser_context_menu_tests_02.html");
    yield addTab(chromeRoot + "browser_context_menu_tests_02.html");

    purgeEventQueue();
    emptyClipboard();

    let browserwin = Browser.selectedTab.browser.contentWindow;

    yield hideContextUI();

    ////////////////////////////////////////////////////////////
    // test for proper context menu positioning when the browser
    // is offset by a notification box.

    yield showNotification();

    // select some text
    let span = browserwin.document.getElementById("text4");
    browserwin.getSelection().selectAllChildren(span);

    // invoke selection context menu
    let promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(browserwin, span);
    yield promise;

    // should be visible and at a specific position
    ok(ContextMenuUI._menuPopup.visible, "is visible");

    let notificationBox = Browser.getNotificationBox();
    let notification = notificationBox.getNotificationWithValue("popup-blocked");
    let notificationHeight = notification.boxObject.height;

    checkContextMenuPositionRange(ContextMenuUI._panel, 0, 15, 175, 190);

    promise = waitForEvent(document, "popuphidden");
    ContextMenuUI.hide();
    yield promise;

    Browser.closeTab(Browser.selectedTab, { forceClose: true });
  }
});

/*
XXX code used to diagnose bug 880739

var observeLogger = {
  observe: function (aSubject, aTopic, aData) {
    info("observeLogger: " + aTopic);
  },
  QueryInterface: function (aIID) {
    if (!aIID.equals(Ci.nsIObserver) &&
        !aIID.equals(Ci.nsISupportsWeakReference) &&
        !aIID.equals(Ci.nsISupports)) {
      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
    return this;
  },
  init: function init() {
    Services.obs.addObserver(observeLogger, "dl-start", true);
    Services.obs.addObserver(observeLogger, "dl-done", true);
    Services.obs.addObserver(observeLogger, "dl-failed", true);
    Services.obs.addObserver(observeLogger, "dl-scanning", true);
    Services.obs.addObserver(observeLogger, "dl-blocked", true);
    Services.obs.addObserver(observeLogger, "dl-dirty", true);
    Services.obs.addObserver(observeLogger, "dl-cancel", true);
  },
  shutdown: function shutdown() {
    Services.obs.removeObserver(observeLogger, "dl-start");
    Services.obs.removeObserver(observeLogger, "dl-done");
    Services.obs.removeObserver(observeLogger, "dl-failed");
    Services.obs.removeObserver(observeLogger, "dl-scanning");
    Services.obs.removeObserver(observeLogger, "dl-blocked");
    Services.obs.removeObserver(observeLogger, "dl-dirty");
    Services.obs.removeObserver(observeLogger, "dl-cancel");
  }
}
*/

// Image context menu tests
gTests.push({
  desc: "image context menu",
  setUp: function() {
    // XXX code used to diagnose bug 880739
    //observeLogger.init();
  },
  tearDown: function() {
    // XXX code used to diagnose bug 880739
    //observeLogger.shutdown();
  },
  run: function test() {
    info(chromeRoot + "browser_context_menu_tests_01.html");
    yield addTab(chromeRoot + "browser_context_menu_tests_01.html");

    let win = Browser.selectedTab.browser.contentWindow;

    purgeEventQueue();

    yield hideContextUI();

    // If we don't do this, sometimes the first sendContextMenuClickToWindow
    // will trigger the app bar.
    yield waitForImageLoad(win, "image01");

    ////////////////////////////////////////////////////////////
    // Context menu options
    /*
    XXX disabled temporarily due to bug 880739

    // image01 - 1x1x100x100
    let promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToWindow(win, 10, 10);
    yield promise;

    purgeEventQueue();

    ok(ContextMenuUI._menuPopup.visible, "is visible");

    checkContextUIMenuItemVisibility(["context-save-image-lib",
                                      "context-copy-image",
                                      "context-copy-image-loc",
                                      "context-open-image-tab"]);

    ////////////////////////////////////////////////////////////
    // Save to image library

    let dirSvc = Components.classes["@mozilla.org/file/directory_service;1"]
                           .getService(Components.interfaces.nsIProperties);
    let saveLocationPath = dirSvc.get("Pict", Components.interfaces.nsIFile);
    saveLocationPath.append("image01.png");

    registerCleanupFunction(function () {
      saveLocationPath.remove(false);
    });

    if (saveLocationPath.exists()) {
      info("had to remove old image!");
      saveLocationPath.remove(false);
    }

    let menuItem = document.getElementById("context-save-image-lib");
    ok(menuItem, "menu item exists");
    ok(!menuItem.hidden, "menu item visible");

    // dl-start, dl-failed, dl-scanning, dl-blocked, dl-dirty, dl-cancel
    let downloadPromise = waitForObserver("dl-done", 10000);

    let popupPromise = waitForEvent(document, "popuphidden");
    EventUtils.synthesizeMouse(menuItem, 10, 10, {}, win);
    yield popupPromise;
    yield downloadPromise;

    purgeEventQueue();

    ok(saveLocationPath.exists(), "image saved");
    */
    ////////////////////////////////////////////////////////////
    // Copy image

    let promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToWindow(win, 20, 20);
    yield promise;
    ok(ContextMenuUI._menuPopup.visible, "is visible");

    let menuItem = document.getElementById("context-copy-image");
    ok(menuItem, "menu item exists");
    ok(!menuItem.hidden, "menu item visible");
    let popupPromise = waitForEvent(document, "popuphidden");
    EventUtils.synthesizeMouse(menuItem, 10, 10, {}, win);
    yield popupPromise;

    purgeEventQueue();

    let clip = Cc["@mozilla.org/widget/clipboard;1"].getService(Ci.nsIClipboard);
    let flavors = ["image/png"];
    ok(clip.hasDataMatchingFlavors(flavors, flavors.length, Ci.nsIClipboard.kGlobalClipboard), "clip has my png flavor");

    ////////////////////////////////////////////////////////////
    // Copy image location

    promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToWindow(win, 30, 30);
    yield promise;
    ok(ContextMenuUI._menuPopup.visible, "is visible");

    menuItem = document.getElementById("context-copy-image-loc");
    ok(menuItem, "menu item exists");
    ok(!menuItem.hidden, "menu item visible");
    popupPromise = waitForEvent(document, "popuphidden");
    EventUtils.synthesizeMouse(menuItem, 10, 10, {}, win);
    yield popupPromise;

    purgeEventQueue();

    let clip = Cc["@mozilla.org/widget/clipboard;1"].getService(Ci.nsIClipboard);
    let flavors = ["text/unicode"];
    ok(clip.hasDataMatchingFlavors(flavors, flavors.length, Ci.nsIClipboard.kGlobalClipboard), "clip has my text flavor");

    let xfer = Cc["@mozilla.org/widget/transferable;1"].
               createInstance(Ci.nsITransferable);
    xfer.init(null);
    xfer.addDataFlavor("text/unicode");
    clip.getData(xfer, Ci.nsIClipboard.kGlobalClipboard);
    let str = new Object();
    let strLength = new Object();
    xfer.getTransferData("text/unicode", str, strLength);
    str = str.value.QueryInterface(Components.interfaces.nsISupportsString);

    ok(str == chromeRoot + "res/image01.png", "url copied");

    ////////////////////////////////////////////////////////////
    // Open image in new tab

    promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToWindow(win, 40, 40);
    yield promise;
    ok(ContextMenuUI._menuPopup.visible, "is visible");

    menuItem = document.getElementById("context-open-image-tab");
    ok(menuItem, "menu item exists");
    ok(!menuItem.hidden, "menu item visible");
    let tabPromise = waitForEvent(document, "TabOpen");
    popupPromise = waitForEvent(document, "popuphidden");
    EventUtils.synthesizeMouse(menuItem, 10, 10, {}, win);
    yield popupPromise;
    let event = yield tabPromise;

    purgeEventQueue();

    let imagetab = Browser.getTabFromChrome(event.originalTarget);
    ok(imagetab != null, "tab created");

    Browser.closeTab(imagetab, { forceClose: true });
  }
});

gTests.push({
  desc: "tests for subframe positioning",
  run: function test() {
    info(chromeRoot + "browser_context_menu_tests_03.html");
    yield addTab(chromeRoot + "browser_context_menu_tests_03.html");

    let win = Browser.selectedTab.browser.contentWindow;

    // Sometimes the context ui is visible, sometimes it isn't.
    try {
      yield waitForCondition(function () {
        return ContextUI.isVisible;
      }, 500, 50);
    } catch (ex) {}

    ContextUI.dismiss();

    let frame1 = win.document.getElementById("frame1");
    let link1 = frame1.contentDocument.getElementById("link1");

    let promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(frame1.contentDocument.defaultView, link1, 85, 10);
    yield promise;

    // should be visible
    ok(ContextMenuUI._menuPopup.visible, "is visible");

    checkContextMenuPositionRange(ContextMenuUI._panel, 265, 280, 175, 190);

    promise = waitForEvent(document, "popuphidden");
    ContextMenuUI.hide();
    yield promise;

    frame1.contentDocument.defaultView.scrollBy(0, 200);

    promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(frame1.contentDocument.defaultView, link1, 85, 10);
    yield promise;

    // should be visible
    ok(ContextMenuUI._menuPopup.visible, "is visible");

    checkContextMenuPositionRange(ContextMenuUI._panel, 265, 280, 95, 110);

    promise = waitForEvent(document, "popuphidden");
    ContextMenuUI.hide();
    yield promise;

    let rlink1 = win.document.getElementById("rlink1");

    promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(win, rlink1, 40, 10);
    yield promise;

    // should be visible
    ok(ContextMenuUI._menuPopup.visible, "is visible");

    checkContextMenuPositionRange(ContextMenuUI._panel, 295, 310, 540, 555);

    promise = waitForEvent(document, "popuphidden");
    ContextMenuUI.hide();
    yield promise;

    win.scrollBy(0, 200);

    promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(win, rlink1, 40, 10);
    yield promise;

    // should be visible
    ok(ContextMenuUI._menuPopup.visible, "is visible");

    checkContextMenuPositionRange(ContextMenuUI._panel, 295, 310, 340, 355);

    promise = waitForEvent(document, "popuphidden");
    ContextMenuUI.hide();
    yield promise;

    let link2 = frame1.contentDocument.getElementById("link2");

    promise = waitForEvent(document, "popupshown");
    sendContextMenuClickToElement(frame1.contentDocument.defaultView, link2, 85, 10);
    yield promise;

    // should be visible
    ok(ContextMenuUI._menuPopup.visible, "is visible");

    checkContextMenuPositionRange(ContextMenuUI._panel, 265, 280, 110, 125);

    promise = waitForEvent(document, "popuphidden");
    ContextMenuUI.hide();
    yield promise;

    Browser.closeTab(Browser.selectedTab, { forceClose: true });
  }
});

function reopenSetUp() {
  info(chromeRoot + "browser_context_menu_tests_04.html");
  yield addTab(chromeRoot + "browser_context_menu_tests_04.html");

  // Sometimes the context UI won't actually show up.
  // Since we're just normalizing, we don't want waitForCondition
  // to cause an orange, so we're putting a try/catch here.
  try {
    yield waitForCondition(() => ContextUI.isVisible);
    ContextUI.dismiss();
  } catch(e) {}
}

function reopenTearDown() {
  let promise = waitForEvent(document, "popuphidden")
  ContextMenuUI.hide();
  yield promise;
  ok(!ContextMenuUI._menuPopup.visible, "popup is actually hidden");

  Browser.closeTab(Browser.selectedTab, { forceClose: true });
}

function getReopenTest(aElementInputFn, aWindowInputFn) {
  return function () {
    let win = Browser.selectedTab.browser.contentWindow;
    let panel = ContextMenuUI._menuPopup._panel;

    let link1 = win.document.getElementById("text1-link");
    let link2 = win.document.getElementById("text2-link");

    // Show the menu on link 1
    let showpromise = waitForEvent(panel, "popupshown");
    aElementInputFn(win, link1);

    ok((yield showpromise), "popupshown event fired");
    ok(ContextMenuUI._menuPopup.visible, "initial popup is visible");

    // Show the menu on link 2
    let hidepromise = waitForEvent(panel, "popuphidden");
    showpromise = waitForEvent(panel, "popupshown");
    aElementInputFn(win, link2);

    ok((yield hidepromise), "popuphidden event fired");
    ok((yield showpromise), "popupshown event fired");
    ok(ContextMenuUI._menuPopup.visible, "popup is still visible");

    // Hide the menu
    hidepromise = waitForEvent(panel, "popuphidden")
    aWindowInputFn(win, 10, 10);

    ok((yield hidepromise), "popuphidden event fired");
    ok(!ContextMenuUI._menuPopup.visible, "popup is no longer visible");
  }
}

gTests.push({
  desc: "bug 856264 - mouse - context menu should reopen on other links",
  setUp: reopenSetUp,
  tearDown: reopenTearDown,
  run: getReopenTest(sendContextMenuMouseClickToElement, sendMouseClick)
});

gTests.push({
  desc: "bug 856264 - touch - context menu should reopen on other links",
  setUp: reopenSetUp,
  tearDown: reopenTearDown,
  run: getReopenTest(sendContextMenuClickToElement, sendTap)
});

function test() {
  runTests();
}
