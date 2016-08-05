/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* Bug 699130 */

"use strict";

var WebConsoleUtils = require("devtools/client/webconsole/utils").Utils;
var DEVTOOLS_CHROME_ENABLED = "devtools.chrome.enabled";

function test()
{
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  Services.prefs.setBoolPref(DEVTOOLS_CHROME_ENABLED, false);
  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    openScratchpad(runTests);
  }, true);

  content.location = "data:text/html,test Edit menu updates Scratchpad - bug 699130";
}

function runTests()
{
  let sp = gScratchpadWindow.Scratchpad;
  let doc = gScratchpadWindow.document;
  let winUtils = gScratchpadWindow.QueryInterface(Ci.nsIInterfaceRequestor).
                 getInterface(Ci.nsIDOMWindowUtils);
  let OS = Services.appinfo.OS;

  info("will test the Edit menu");

  let pass = 0;

  sp.setText("bug 699130: hello world! (edit menu)");

  let editMenu = doc.getElementById("sp-edit-menu");
  ok(editMenu, "the Edit menu");
  let menubar = editMenu.parentNode;
  ok(menubar, "menubar found");

  let editMenuIndex = -1;
  for (let i = 0; i < menubar.children.length; i++) {
    if (menubar.children[i] === editMenu) {
      editMenuIndex = i;
      break;
    }
  }
  isnot(editMenuIndex, -1, "Edit menu index is correct");

  let menuPopup = editMenu.menupopup;
  ok(menuPopup, "the Edit menupopup");
  let cutItem = doc.getElementById("menu_cut");
  ok(cutItem, "the Cut menuitem");
  let pasteItem = doc.getElementById("menu_paste");
  ok(pasteItem, "the Paste menuitem");

  let anchor = doc.documentElement;
  let isContextMenu = false;

  let oldVal = sp.editor.getText();

  let testSelfXss = function (oldVal) {
    // Self xss prevention tests (bug 994134)
    info("Self xss paste tests");
    is(WebConsoleUtils.usageCount, 0, "Test for usage count getter");
    let notificationbox = doc.getElementById("scratchpad-notificationbox");
    let notification = notificationbox.getNotificationWithValue("selfxss-notification");
    ok(notification, "Self-xss notification shown");
    is(oldVal, sp.editor.getText(), "Paste blocked by self-xss prevention");
    Services.prefs.setIntPref("devtools.selfxss.count", 10);
    notificationbox.removeAllNotifications(true);
    openMenu(10, 10, firstShow);
  };

  let openMenu = function (aX, aY, aCallback) {
    if (!editMenu || OS != "Darwin") {
      menuPopup.addEventListener("popupshown", function onPopupShown() {
        menuPopup.removeEventListener("popupshown", onPopupShown, false);
        executeSoon(aCallback);
      }, false);
    }

    executeSoon(function () {
      if (editMenu) {
        if (OS == "Darwin") {
          winUtils.forceUpdateNativeMenuAt(editMenuIndex);
          executeSoon(aCallback);
        } else {
          editMenu.open = true;
        }
      } else {
        menuPopup.openPopup(anchor, "overlap", aX, aY, isContextMenu, false);
      }
    });
  };

  let closeMenu = function (aCallback) {
    if (!editMenu || OS != "Darwin") {
      menuPopup.addEventListener("popuphidden", function onPopupHidden() {
        menuPopup.removeEventListener("popuphidden", onPopupHidden, false);
        executeSoon(aCallback);
      }, false);
    }

    executeSoon(function () {
      if (editMenu) {
        if (OS == "Darwin") {
          winUtils.forceUpdateNativeMenuAt(editMenuIndex);
          executeSoon(aCallback);
        } else {
          editMenu.open = false;
        }
      } else {
        menuPopup.hidePopup();
      }
    });
  };

  let firstShow = function () {
    ok(!cutItem.hasAttribute("disabled"), "cut menuitem is enabled");
    closeMenu(firstHide);
  };

  let firstHide = function () {
    sp.editor.setSelection({ line: 0, ch: 0 }, { line: 0, ch: 10 });
    openMenu(11, 11, showAfterSelect);
  };

  let showAfterSelect = function () {
    ok(!cutItem.hasAttribute("disabled"), "cut menuitem is enabled after select");
    closeMenu(hideAfterSelect);
  };

  let hideAfterSelect = function () {
    sp.editor.on("change", onCut);
    waitForFocus(function () {
      let selectedText = sp.editor.getSelection();
      ok(selectedText.length > 0, "non-empty selected text will be cut");

      EventUtils.synthesizeKey("x", {accelKey: true}, gScratchpadWindow);
    }, gScratchpadWindow);
  };

  let onCut = function () {
    sp.editor.off("change", onCut);
    openMenu(12, 12, showAfterCut);
  };

  let showAfterCut = function () {
    ok(!cutItem.hasAttribute("disabled"), "cut menuitem is enabled after cut");
    ok(!pasteItem.hasAttribute("disabled"), "paste menuitem is enabled after cut");
    closeMenu(hideAfterCut);
  };

  let hideAfterCut = function () {
    waitForFocus(function () {
      sp.editor.on("change", onPaste);
      EventUtils.synthesizeKey("v", {accelKey: true}, gScratchpadWindow);
    }, gScratchpadWindow);
  };

  let onPaste = function () {
    sp.editor.off("change", onPaste);
    openMenu(13, 13, showAfterPaste);
  };

  let showAfterPaste = function () {
    ok(!cutItem.hasAttribute("disabled"), "cut menuitem is enabled after paste");
    ok(!pasteItem.hasAttribute("disabled"), "paste menuitem is enabled after paste");
    closeMenu(hideAfterPaste);
  };

  let hideAfterPaste = function () {
    if (pass == 0) {
      pass++;
      testContextMenu();
    } else {
      Services.prefs.clearUserPref(DEVTOOLS_CHROME_ENABLED);
      finish();
    }
  };

  let testContextMenu = function () {
    info("will test the context menu");

    editMenu = null;
    isContextMenu = true;

    menuPopup = doc.getElementById("scratchpad-text-popup");
    ok(menuPopup, "the context menupopup");
    cutItem = doc.getElementById("cMenu_cut");
    ok(cutItem, "the Cut menuitem");
    pasteItem = doc.getElementById("cMenu_paste");
    ok(pasteItem, "the Paste menuitem");

    sp.setText("bug 699130: hello world! (context menu)");
    openMenu(10, 10, firstShow);
  };
  waitForFocus(function () {
    WebConsoleUtils.usageCount = 0;
    EventUtils.synthesizeKey("v", {accelKey: true}, gScratchpadWindow);
    testSelfXss(oldVal);
  }, gScratchpadWindow);
}
