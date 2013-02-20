/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

// Image context menu tests
gTests.push({
  desc: "image context menu",
  run: function test() {
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

    info(chromeRoot + "browser_context_menu_tests_01.html");
    yield addTab(chromeRoot + "browser_context_menu_tests_01.html");

    let win = Browser.selectedTab.browser.contentWindow;

    yield hideContextUI();

    ////////////////////////////////////////////////////////////
    // Context menu options

    // image01 - 1x1x100x100
    let promise = waitForEvent(document, "popupshown");
    sendContextMenuClick(win, 10, 10);
    yield promise;
    ok(promise && !(promise instanceof Error), "promise error");

    purgeEventQueue();

    ok(ContextMenuUI._menuPopup._visible, "is visible");

    checkContextUIMenuItemCount(4);

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

    let downloadPromise = waitForObserver("dl-done");
    let popupPromise = waitForEvent(document, "popuphidden");
    EventUtils.synthesizeMouse(menuItem, 10, 10, {}, win);
    yield popupPromise;
    ok(popupPromise && !(popupPromise instanceof Error), "promise error");
    yield downloadPromise;
    ok(downloadPromise && !(downloadPromise instanceof Error), "promise error");

    purgeEventQueue();

    ok(saveLocationPath.exists(), "image saved");

    ////////////////////////////////////////////////////////////
    // Copy image

    let promise = waitForEvent(document, "popupshown");
    sendContextMenuClick(win, 30, 30);
    yield promise;
    ok(promise && !(promise instanceof Error), "promise error");
    ok(ContextMenuUI._menuPopup._visible, "is visible");

    menuItem = document.getElementById("context-copy-image");
    ok(menuItem, "menu item exists");
    ok(!menuItem.hidden, "menu item visible");
    popupPromise = waitForEvent(document, "popuphidden");
    EventUtils.synthesizeMouse(menuItem, 10, 10, {}, win);
    yield popupPromise;
    ok(popupPromise && !(popupPromise instanceof Error), "promise error");

    purgeEventQueue();

    let clip = Cc["@mozilla.org/widget/clipboard;1"].getService(Ci.nsIClipboard);
    let flavors = ["image/png"];
    ok(clip.hasDataMatchingFlavors(flavors, flavors.length, Ci.nsIClipboard.kGlobalClipboard), "clip has my flavor");

    ////////////////////////////////////////////////////////////
    // Copy image location

    promise = waitForEvent(document, "popupshown");
    sendContextMenuClick(win, 60, 60);
    yield promise;
    ok(promise && !(promise instanceof Error), "promise error");
    ok(ContextMenuUI._menuPopup._visible, "is visible");

    menuItem = document.getElementById("context-copy-image-loc");
    ok(menuItem, "menu item exists");
    ok(!menuItem.hidden, "menu item visible");
    popupPromise = waitForEvent(document, "popuphidden");
    EventUtils.synthesizeMouse(menuItem, 10, 10, {}, win);
    yield popupPromise;
    ok(popupPromise && !(popupPromise instanceof Error), "promise error");

    purgeEventQueue();

    let clip = Cc["@mozilla.org/widget/clipboard;1"].getService(Ci.nsIClipboard);
    let flavors = ["text/unicode"];
    ok(clip.hasDataMatchingFlavors(flavors, flavors.length, Ci.nsIClipboard.kGlobalClipboard), "clip has my flavor");

    let xfer = Cc["@mozilla.org/widget/transferable;1"].
               createInstance(Ci.nsITransferable);
    xfer.init(null);
    xfer.addDataFlavor("text/unicode");
    clip.getData(xfer, Ci.nsIClipboard.kGlobalClipboard);
    let str = new Object();
    let strLength = new Object();
    xfer.getTransferData("text/unicode", str, strLength);
    str = str.value.QueryInterface(Components.interfaces.nsISupportsString);
    ok(str == "chrome://mochitests/content/metro/res/image01.png", "url copied");

    ////////////////////////////////////////////////////////////
    // Open image in new tab

    promise = waitForEvent(document, "popupshown");
    sendContextMenuClick(win, 60, 60);
    yield promise;
    ok(promise && !(promise instanceof Error), "promise error");
    ok(ContextMenuUI._menuPopup._visible, "is visible");

    menuItem = document.getElementById("context-open-image-tab");
    ok(menuItem, "menu item exists");
    ok(!menuItem.hidden, "menu item visible");
    let tabPromise = waitForEvent(document, "TabOpen");
    popupPromise = waitForEvent(document, "popuphidden");
    EventUtils.synthesizeMouse(menuItem, 10, 10, {}, win);
    yield popupPromise;
    let event = yield tabPromise;
    ok(popupPromise && !(popupPromise instanceof Error), "promise error");
    ok(tabPromise && !(tabPromise instanceof Error), "promise error");

    purgeEventQueue();

    let imagetab = Browser.getTabFromChrome(event.originalTarget);
    ok(imagetab != null, "tab created");
    ok(imagetab.browser.currentURI.spec == "chrome://mochitests/content/metro/res/image01.png", "tab location");

    Browser.closeTab(imagetab);
  }
});

function test() {
  runTests();
}
