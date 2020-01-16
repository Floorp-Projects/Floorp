/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarProvider: "resource:///modules/UrlbarUtils.jsm",
  UrlbarProvidersManager: "resource:///modules/UrlbarProvidersManager.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

function copyToClipboard(str) {
  return new Promise((resolve, reject) => {
    waitForClipboard(
      str,
      () => {
        Cc["@mozilla.org/widget/clipboardhelper;1"]
          .getService(Ci.nsIClipboardHelper)
          .copyString(str);
      },
      resolve,
      reject
    );
  });
}

// Each test is a function that executes an urlbar action and returns the
// expected event object, or null if no event is expected.
const tests = [
  /*
   * Engagement tests.
   */
  async function() {
    info("Type something, press Enter.");
    gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    await promiseAutocompleteResultPopup("x", window, true);
    EventUtils.synthesizeKey("VK_RETURN");
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "1",
        selIndex: "0",
        selType: "search",
      },
    };
  },

  async function() {
    info("Paste something, press Enter.");
    gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    await SimpleTest.promiseClipboardChange("test", () => {
      clipboardHelper.copyString("test");
    });
    document.commandDispatcher
      .getControllerForCommand("cmd_paste")
      .doCommand("cmd_paste");
    EventUtils.synthesizeKey("VK_RETURN");
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "pasted",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "4",
        selIndex: "0",
        selType: "search",
      },
    };
  },

  async function() {
    info("Type something, click one-off.");
    gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    await promiseAutocompleteResultPopup("moz", window, true);
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
    UrlbarTestUtils.getOneOffSearchButtons(window).selectedButton.click();
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "click",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "3",
        selIndex: "0",
        selType: "oneoff",
      },
    };
  },

  async function() {
    info("Type something, select one-off, Enter.");
    gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    await promiseAutocompleteResultPopup("moz", window, true);
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true });
    Assert.ok(UrlbarTestUtils.getOneOffSearchButtons(window).selectedButton);
    EventUtils.synthesizeKey("VK_RETURN");
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "3",
        selIndex: "0",
        selType: "oneoff",
      },
    };
  },

  async function() {
    info("Type something, ESC, type something else, press Enter.");
    gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    EventUtils.synthesizeKey("x");
    EventUtils.synthesizeKey("VK_ESCAPE");
    EventUtils.synthesizeKey("y");
    EventUtils.synthesizeKey("VK_RETURN");
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "1",
        selIndex: "0",
        selType: "search",
      },
    };
  },

  async function() {
    info("Type a keyword, Enter.");
    gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    await promiseAutocompleteResultPopup("kw test", window, true);
    EventUtils.synthesizeKey("VK_RETURN");
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "7",
        selIndex: "0",
        selType: "keyword",
      },
    };
  },

  async function() {
    let tipProvider = registerTipProvider();
    info("Selecting a tip's main button, enter.");
    gURLBar.search("x");
    await promiseSearchComplete();
    EventUtils.synthesizeKey("KEY_ArrowDown");
    EventUtils.synthesizeKey("KEY_ArrowDown");
    EventUtils.synthesizeKey("VK_RETURN");
    unregisterTipProvider(tipProvider);
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "1",
        selIndex: "1",
        selType: "tip",
      },
    };
  },

  async function() {
    let tipProvider = registerTipProvider();
    info("Selecting a tip's help button, enter.");
    let promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    gURLBar.search("x");
    await promiseSearchComplete();
    EventUtils.synthesizeKey("KEY_ArrowDown");
    EventUtils.synthesizeKey("KEY_ArrowDown");
    EventUtils.synthesizeKey("KEY_ArrowDown");
    EventUtils.synthesizeKey("VK_RETURN");
    await promise;
    unregisterTipProvider(tipProvider);
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "1",
        selIndex: "1",
        selType: "tiphelp",
      },
    };
  },

  async function() {
    info("Type something and canonize");
    gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    await promiseAutocompleteResultPopup("example", window, true);
    EventUtils.synthesizeKey("VK_RETURN", { ctrlKey: true });
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "7",
        selIndex: "0",
        selType: "canonized",
      },
    };
  },

  async function() {
    info("Type something, click on bookmark entry.");
    gURLBar.select();
    let url = "http://example.com/?q=%s";
    let promise = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      false,
      url
    );
    await promiseAutocompleteResultPopup("exa", window, true);
    while (gURLBar.untrimmedValue != url) {
      EventUtils.synthesizeKey("KEY_ArrowDown");
    }
    let element = UrlbarTestUtils.getSelectedRow(window);
    EventUtils.synthesizeMouseAtCenter(element, {});
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "click",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "3",
        selIndex: val => parseInt(val) > 0,
        selType: "bookmark",
      },
    };
  },

  async function() {
    info("Type an autofilled string, Enter.");
    gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    await promiseAutocompleteResultPopup("exa", window, true);
    // Check it's autofilled.
    Assert.equal(gURLBar.selectionStart, 3);
    Assert.equal(gURLBar.selectionEnd, 12);
    EventUtils.synthesizeKey("VK_RETURN");
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "3",
        selIndex: "0",
        selType: "autofill",
      },
    };
  },

  async function() {
    info("Type something, select bookmark entry, Enter.");
    gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    await promiseAutocompleteResultPopup("exa", window, true);
    while (gURLBar.untrimmedValue != "http://example.com/?q=%s") {
      EventUtils.synthesizeKey("KEY_ArrowDown");
    }
    EventUtils.synthesizeKey("VK_RETURN");
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "3",
        selIndex: val => parseInt(val) > 0,
        selType: "bookmark",
      },
    };
  },

  async function() {
    info("Type @, Enter on a keywordoffer");
    gURLBar.select();
    await promiseAutocompleteResultPopup("@", window, true);
    while (gURLBar.untrimmedValue != "@test ") {
      EventUtils.synthesizeKey("KEY_ArrowDown");
    }
    EventUtils.synthesizeKey("VK_RETURN");
    await UrlbarTestUtils.promiseSearchComplete(window);
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "1",
        selIndex: val => parseInt(val) > 0,
        selType: "keywordoffer",
      },
    };
  },

  async function() {
    info("Drop something.");
    let promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    EventUtils.synthesizeDrop(
      document.getElementById("home-button"),
      gURLBar.inputField,
      [[{ type: "text/plain", data: "www.example.com" }]],
      "copy",
      window
    );
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "drop_go",
      value: "dropped",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "15",
        selIndex: "-1",
        selType: "none",
      },
    };
  },

  async function() {
    info("Paste & Go something.");
    let promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    await SimpleTest.promiseClipboardChange("www.example.com", () => {
      clipboardHelper.copyString("www.example.com");
    });
    let inputBox = gURLBar.querySelector("moz-input-box");
    let cxmenu = inputBox.menupopup;
    let cxmenuPromise = BrowserTestUtils.waitForEvent(cxmenu, "popupshown");
    EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {
      type: "contextmenu",
      button: 2,
    });
    await cxmenuPromise;
    let menuitem = inputBox.getMenuItem("paste-and-go");
    EventUtils.synthesizeMouseAtCenter(menuitem, {});
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "paste_go",
      value: "pasted",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "15",
        selIndex: "-1",
        selType: "none",
      },
    };
  },

  // The URLs in the down arrow/openViewOnFocus tests must vary from test to test,
  // else  the first Top Site results will be a switch-to-tab result and a page
  // load will not occur.
  async function() {
    info("Open the panel with DOWN, select with DOWN, Enter.");
    await addTopSite("http://example.org/");
    gURLBar.value = "";
    gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    await UrlbarTestUtils.promisePopupOpen(window, () => {
      EventUtils.synthesizeKey("KEY_ArrowDown", {});
    });
    await UrlbarTestUtils.promiseSearchComplete(window);
    while (gURLBar.untrimmedValue != "http://example.org/") {
      EventUtils.synthesizeKey("KEY_ArrowDown");
    }
    EventUtils.synthesizeKey("VK_RETURN");
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "topsites",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "0",
        selType: "history",
        selIndex: val => parseInt(val) >= 0,
      },
    };
  },

  async function() {
    info("Open the panel with DOWN, click on entry.");
    await addTopSite("http://example.com/");
    gURLBar.value = "";
    gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    await UrlbarTestUtils.promisePopupOpen(window, () => {
      EventUtils.synthesizeKey("KEY_ArrowDown", {});
    });
    while (gURLBar.untrimmedValue != "http://example.com/") {
      EventUtils.synthesizeKey("KEY_ArrowDown");
    }
    let element = UrlbarTestUtils.getSelectedRow(window);
    EventUtils.synthesizeMouseAtCenter(element, {});
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "click",
      value: "topsites",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "0",
        selType: "history",
        selIndex: "0",
      },
    };
  },

  async function() {
    info("Open the panel with dropmarker, type something, Enter.");
    Services.prefs.setBoolPref("browser.urlbar.openViewOnFocus", false);
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: "about:blank" },
      async browser => {
        gURLBar.select();
        let promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
        await UrlbarTestUtils.promisePopupOpen(window, () => {
          EventUtils.synthesizeMouseAtCenter(gURLBar.dropmarker, {}, window);
        });
        await promiseAutocompleteResultPopup("x", window, true);
        EventUtils.synthesizeKey("VK_RETURN");
        await promise;
      }
    );
    Services.prefs.clearUserPref("browser.urlbar.openViewOnFocus");
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "topsites",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "1",
        selType: "search",
        selIndex: "0",
      },
    };
  },

  // The URLs in the openViewOnFocus tests must vary from test to test, else
  // the first Top Site results will be a switch-to-tab result and a page load
  // will not occur.
  async function() {
    info(
      "With pageproxystate=valid, open the panel with openViewOnFocus, select with DOWN, Enter."
    );
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.urlbar.update1", true],
        ["browser.urlbar.openViewOnFocus", true],
      ],
    });
    await addTopSite("http://example.org/");
    gURLBar.value = "";
    let promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    await UrlbarTestUtils.promisePopupOpen(window, () => {
      window.document.getElementById("Browser:OpenLocation").doCommand();
    });
    await SpecialPowers.popPrefEnv();
    await UrlbarTestUtils.promiseSearchComplete(window);
    while (gURLBar.untrimmedValue != "http://example.org/") {
      EventUtils.synthesizeKey("KEY_ArrowDown");
    }
    EventUtils.synthesizeKey("VK_RETURN");
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "topsites",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "0",
        selType: "history",
        selIndex: val => parseInt(val) >= 0,
      },
    };
  },

  async function() {
    info(
      "With pageproxystate=valid, open the panel with openViewOnFocus, click on entry."
    );

    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.urlbar.update1", true],
        ["browser.urlbar.openViewOnFocus", true],
      ],
    });
    await addTopSite("http://example.com/");
    gURLBar.value = "";
    let promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    await UrlbarTestUtils.promisePopupOpen(window, () => {
      window.document.getElementById("Browser:OpenLocation").doCommand();
    });
    await SpecialPowers.popPrefEnv();
    await UrlbarTestUtils.promiseSearchComplete(window);
    while (gURLBar.untrimmedValue != "http://example.com/") {
      EventUtils.synthesizeKey("KEY_ArrowDown");
    }
    let element = UrlbarTestUtils.getSelectedRow(window);
    EventUtils.synthesizeMouseAtCenter(element, {});
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "click",
      value: "topsites",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "0",
        selType: "history",
        selIndex: "0",
      },
    };
  },

  async function() {
    info(
      "With pageproxystate=invalid, open the panel with openViewOnFocus, Enter."
    );
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.urlbar.update1", true],
        ["browser.urlbar.openViewOnFocus", true],
      ],
    });
    await addTopSite("http://example.org/");
    gURLBar.value = "example.org";
    gURLBar.setAttribute("pageproxystate", "invalid");
    let promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    await UrlbarTestUtils.promisePopupOpen(window, () => {
      window.document.getElementById("Browser:OpenLocation").doCommand();
    });
    await SpecialPowers.popPrefEnv();
    await UrlbarTestUtils.promiseSearchComplete(window);
    EventUtils.synthesizeKey("VK_RETURN");
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "11",
        selType: "autofill",
        selIndex: "0",
      },
    };
  },

  async function() {
    info(
      "With pageproxystate=invalid, open the panel with openViewOnFocus, click on entry."
    );
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.urlbar.update1", true],
        ["browser.urlbar.openViewOnFocus", true],
      ],
    });
    // This value must be different from the previous test, or else the
    // Megabar's "retained results" feature will interfere with openViewOnFocus.
    // This issue will be addressed in bug 1601052.
    gURLBar.value = "example.com";
    gURLBar.setAttribute("pageproxystate", "invalid");
    let promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    await UrlbarTestUtils.promisePopupOpen(window, () => {
      window.document.getElementById("Browser:OpenLocation").doCommand();
    });
    await SpecialPowers.popPrefEnv();
    await UrlbarTestUtils.promiseSearchComplete(window);
    let element = UrlbarTestUtils.getSelectedRow(window);
    EventUtils.synthesizeMouseAtCenter(element, {});
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "click",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "11",
        selType: "autofill",
        selIndex: "0",
      },
    };
  },

  async function() {
    info("Retained result: type, blur, focus, confirm.");
    await SpecialPowers.pushPrefEnv({
      set: [["browser.urlbar.update1", true]],
    });
    await promiseAutocompleteResultPopup("search", window, true);
    await UrlbarTestUtils.promisePopupClose(window, () => {
      gURLBar.blur();
    });
    await UrlbarTestUtils.promisePopupOpen(window, () => {
      document.getElementById("Browser:OpenLocation").doCommand();
    });
    await UrlbarTestUtils.promiseSearchComplete(window);
    let promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    EventUtils.synthesizeKey("VK_RETURN");
    await promise;
    await SpecialPowers.popPrefEnv();
    return [
      {
        category: "urlbar",
        method: "abandonment",
        object: "blur",
        value: "typed",
        extra: {
          elapsed: val => parseInt(val) > 0,
          numChars: "6",
        },
      },
      {
        category: "urlbar",
        method: "engagement",
        object: "enter",
        value: "returned",
        extra: {
          elapsed: val => parseInt(val) > 0,
          numChars: "6",
          selType: "search",
          selIndex: "0",
        },
      },
    ];
  },

  async function() {
    info("Sanity check we are not stuck on 'returned'");
    gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    await promiseAutocompleteResultPopup("x", window, true);
    EventUtils.synthesizeKey("VK_RETURN");
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "1",
        selIndex: "0",
        selType: "search",
      },
    };
  },

  async function() {
    info("Retained result: type, blur, focus, backspace, type, confirm.");
    await SpecialPowers.pushPrefEnv({
      set: [["browser.urlbar.update1", true]],
    });
    await promiseAutocompleteResultPopup("search", window, true);
    await UrlbarTestUtils.promisePopupClose(window, () => {
      gURLBar.blur();
    });
    await UrlbarTestUtils.promisePopupOpen(window, () => {
      document.getElementById("Browser:OpenLocation").doCommand();
    });
    EventUtils.synthesizeKey("VK_RIGHT");
    EventUtils.synthesizeKey("VK_BACK_SPACE");
    EventUtils.synthesizeKey("x");
    await UrlbarTestUtils.promiseSearchComplete(window);
    let promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    EventUtils.synthesizeKey("VK_RETURN");
    await promise;
    await SpecialPowers.popPrefEnv();
    return [
      {
        category: "urlbar",
        method: "abandonment",
        object: "blur",
        value: "typed",
        extra: {
          elapsed: val => parseInt(val) > 0,
          numChars: "6",
        },
      },
      {
        category: "urlbar",
        method: "engagement",
        object: "enter",
        value: "returned",
        extra: {
          elapsed: val => parseInt(val) > 0,
          numChars: "6",
          selType: "search",
          selIndex: "0",
        },
      },
    ];
  },

  async function() {
    info("Retained result: type, blur, focus, type (overwrite), confirm.");
    await SpecialPowers.pushPrefEnv({
      set: [["browser.urlbar.update1", true]],
    });
    await promiseAutocompleteResultPopup("search", window, true);
    await UrlbarTestUtils.promisePopupClose(window, () => {
      gURLBar.blur();
    });
    await UrlbarTestUtils.promisePopupOpen(window, () => {
      document.getElementById("Browser:OpenLocation").doCommand();
    });
    EventUtils.synthesizeKey("x");
    await UrlbarTestUtils.promiseSearchComplete(window);
    let promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    EventUtils.synthesizeKey("VK_RETURN");
    await promise;
    await SpecialPowers.popPrefEnv();
    return [
      {
        category: "urlbar",
        method: "abandonment",
        object: "blur",
        value: "typed",
        extra: {
          elapsed: val => parseInt(val) > 0,
          numChars: "6",
        },
      },
      {
        category: "urlbar",
        method: "engagement",
        object: "enter",
        value: "restarted",
        extra: {
          elapsed: val => parseInt(val) > 0,
          numChars: "1",
          selType: "search",
          selIndex: "0",
        },
      },
    ];
  },

  async function() {
    info("Sanity check we are not stuck on 'restarted'");
    gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    await promiseAutocompleteResultPopup("x", window, true);
    EventUtils.synthesizeKey("VK_RETURN");
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "1",
        selIndex: "0",
        selType: "search",
      },
    };
  },

  /*
   * Abandonment tests.
   */

  async function() {
    info("Type something, blur.");
    gURLBar.select();
    EventUtils.synthesizeKey("x");
    gURLBar.blur();
    return {
      category: "urlbar",
      method: "abandonment",
      object: "blur",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "1",
      },
    };
  },

  async function() {
    info("Open the panel with DOWN, don't type, blur it.");
    gURLBar.value = "";
    gURLBar.select();
    await UrlbarTestUtils.promisePopupOpen(window, () => {
      EventUtils.synthesizeKey("KEY_ArrowDown", {});
    });
    gURLBar.blur();
    return {
      category: "urlbar",
      method: "abandonment",
      object: "blur",
      value: "topsites",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "0",
      },
    };
  },

  async function() {
    info("Open the panel with dropmarker, type something, blur it.");
    Services.prefs.setBoolPref("browser.urlbar.openViewOnFocus", false);
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: "about:blank" },
      async browser => {
        gURLBar.select();
        await UrlbarTestUtils.promisePopupOpen(window, () => {
          EventUtils.synthesizeMouseAtCenter(gURLBar.dropmarker, {}, window);
        });
        EventUtils.synthesizeKey("x");
        gURLBar.blur();
      }
    );
    Services.prefs.clearUserPref("browser.urlbar.openViewOnFocus");
    return {
      category: "urlbar",
      method: "abandonment",
      object: "blur",
      value: "topsites",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "1",
      },
    };
  },

  async function() {
    info(
      "With pageproxystate=valid, open the panel with openViewOnFocus, don't type, blur it."
    );
    gURLBar.value = "";
    Services.prefs.setBoolPref("browser.urlbar.openViewOnFocus", true);
    await UrlbarTestUtils.promisePopupOpen(window, () => {
      window.document.getElementById("Browser:OpenLocation").doCommand();
    });
    Services.prefs.clearUserPref("browser.urlbar.openViewOnFocus");
    gURLBar.blur();
    return {
      category: "urlbar",
      method: "abandonment",
      object: "blur",
      value: "topsites",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "0",
      },
    };
  },

  async function() {
    info(
      "With pageproxystate=invalid, open the panel with openViewOnFocus, don't type, blur it."
    );
    gURLBar.value = "mochi.test";
    gURLBar.setAttribute("pageproxystate", "invalid");
    Services.prefs.setBoolPref("browser.urlbar.openViewOnFocus", true);
    await UrlbarTestUtils.promisePopupOpen(window, () => {
      window.document.getElementById("Browser:OpenLocation").doCommand();
    });
    Services.prefs.clearUserPref("browser.urlbar.openViewOnFocus");
    gURLBar.blur();
    return {
      category: "urlbar",
      method: "abandonment",
      object: "blur",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "10",
      },
    };
  },

  /*
   * No event tests.
   */

  async function() {
    info("Type something, click on search settings.");
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: "about:blank" },
      async browser => {
        gURLBar.select();
        let promise = BrowserTestUtils.browserLoaded(browser);
        await promiseAutocompleteResultPopup("x", window, true);
        UrlbarTestUtils.getOneOffSearchButtons(window).settingsButton.click();
        await promise;
      }
    );
    return null;
  },

  async function() {
    info("Type something, Up, Enter on search settings.");
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: "about:blank" },
      async browser => {
        gURLBar.select();
        let promise = BrowserTestUtils.browserLoaded(browser);
        await promiseAutocompleteResultPopup("x", window, true);
        EventUtils.synthesizeKey("KEY_ArrowUp");
        Assert.ok(
          UrlbarTestUtils.getOneOffSearchButtons(
            window
          ).selectedButton.classList.contains("search-setting-button-compact"),
          "Should have selected the settings button"
        );
        EventUtils.synthesizeKey("VK_RETURN");
        await promise;
      }
    );
    return null;
  },
];

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.eventTelemetry.enabled", true],
      ["browser.urlbar.suggest.searches", true],
    ],
  });
  // Create a new search engine and mark it as default
  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + "searchSuggestionEngine.xml"
  );
  let oldDefaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(engine);
  await Services.search.moveEngine(engine, 0);

  let aliasEngine = await Services.search.addEngineWithDetails("Test", {
    alias: "@test",
    template: "http://example.com/?search={searchTerms}",
  });

  // Add a bookmark and a keyword.
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://example.com/?q=%s",
    title: "test",
  });
  await PlacesUtils.keywords.insert({
    keyword: "kw",
    url: "http://example.com/?q=%s",
  });
  await PlacesTestUtils.addVisits([
    {
      uri: "http://mochi.test:8888/",
      transition: PlacesUtils.history.TRANSITIONS.TYPED,
    },
  ]);

  registerCleanupFunction(async function() {
    await Services.search.setDefault(oldDefaultEngine);
    await Services.search.removeEngine(aliasEngine);
    await PlacesUtils.keywords.remove("kw");
    await PlacesUtils.bookmarks.remove(bm);
    await PlacesUtils.history.clear();
  });

  // This is not necessary after each loop, because assertEvents does it.
  Services.telemetry.clearEvents();

  for (let i = 0; i < tests.length; i++) {
    info(`Running test at index ${i}`);
    let testFn = tests[i];
    let events = await testFn();
    if (!Array.isArray(events)) {
      events = [events].filter(e => !!e);
    }
    // Always blur to ensure it's not accounted as an additional abandonment.
    gURLBar.blur();
    TelemetryTestUtils.assertEvents(events, { category: "urlbar" });
  }
});

/**
 * Replaces the contents of Top Sites with the specified site.
 * @param {string} site
 *   A site to add to Top Sites.
 */
async function addTopSite(site) {
  await PlacesUtils.history.clear();
  for (let i = 0; i < 5; i++) {
    await PlacesTestUtils.addVisits(site);
  }

  await updateTopSites(sites => sites && sites[0] && sites[0].url == site);
}

function registerTipProvider() {
  let provider = new TipTestProvider(tipMatches);
  UrlbarProvidersManager.registerProvider(provider);
  return provider;
}

function unregisterTipProvider(provider) {
  UrlbarProvidersManager.unregisterProvider(provider);
}

/**
 * A test tip provider. See browser_tip_selection.js.
 */
class TipTestProvider extends UrlbarProvider {
  constructor(matches) {
    super();
    this._matches = matches;
  }
  get name() {
    return "TipTestProvider";
  }
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
  }
  isActive(context) {
    return true;
  }
  isRestricting(context) {
    return true;
  }
  async startQuery(context, addCallback) {
    this._context = context;
    for (const match of this._matches) {
      addCallback(this, match);
    }
  }
  cancelQuery(context) {}
  pickResult(result) {}
}

let tipMatches = [
  new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.URL,
    UrlbarUtils.RESULT_SOURCE.HISTORY,
    { url: "http://mozilla.org/a" }
  ),
  new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.TIP,
    UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
    {
      text: "This is a test intervention.",
      buttonText: "Done",
      data: "test",
      helpUrl: "about:about",
    }
  ),
  new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.URL,
    UrlbarUtils.RESULT_SOURCE.HISTORY,
    { url: "http://mozilla.org/b" }
  ),
  new UrlbarResult(
    UrlbarUtils.RESULT_TYPE.URL,
    UrlbarUtils.RESULT_SOURCE.HISTORY,
    { url: "http://mozilla.org/c" }
  ),
];
