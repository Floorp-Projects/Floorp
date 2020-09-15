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
  async function(win) {
    info("Type something, press Enter.");
    win.gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "x",
      fireInputEvent: true,
    });
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "1",
        numWords: "1",
        selIndex: "0",
        selType: "search",
      },
    };
  },

  async function(win) {
    info("Type a multi-word query, press Enter.");
    win.gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "multi word query ",
      fireInputEvent: true,
    });
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "17",
        numWords: "3",
        selIndex: "0",
        selType: "search",
      },
    };
  },

  async function(win) {
    info("Paste something, press Enter.");
    win.gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    await SimpleTest.promiseClipboardChange("test", () => {
      clipboardHelper.copyString("test");
    });
    win.document.commandDispatcher
      .getControllerForCommand("cmd_paste")
      .doCommand("cmd_paste");
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "pasted",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "4",
        numWords: "1",
        selIndex: "0",
        selType: "search",
      },
    };
  },

  async function(win) {
    info("Type something, click one-off. Update2 disabled.");
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.urlbar.update2", false],
        ["browser.urlbar.update2.oneOffsRefresh", false],
      ],
    });
    win.gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "moz",
      fireInputEvent: true,
    });
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true }, win);
    UrlbarTestUtils.getOneOffSearchButtons(win).selectedButton.click();
    await promise;
    await SpecialPowers.popPrefEnv();
    return {
      category: "urlbar",
      method: "engagement",
      object: "click",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "3",
        numWords: "1",
        selIndex: "0",
        selType: "oneoff",
      },
    };
  },

  async function(win) {
    info("Type something, click one-off and press enter.");
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.urlbar.update2", true],
        ["browser.urlbar.update2.oneOffsRefresh", true],
      ],
    });
    win.gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "test",
      fireInputEvent: true,
    });

    let searchPromise = UrlbarTestUtils.promiseSearchComplete(win);
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true }, win);
    let selectedOneOff = UrlbarTestUtils.getOneOffSearchButtons(win)
      .selectedButton;
    selectedOneOff.click();
    await searchPromise;
    await UrlbarTestUtils.assertSearchMode(window, {
      engineName: selectedOneOff.engine.name,
      entry: "oneoff",
    });

    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    await promise;
    await SpecialPowers.popPrefEnv();
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "4",
        numWords: "1",
        selIndex: "0",
        selType: "search",
      },
    };
  },

  async function(win) {
    info("Type something, select one-off, Enter. Update2 disabled.");
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.urlbar.update2", false],
        ["browser.urlbar.update2.oneOffsRefresh", false],
      ],
    });
    win.gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "moz",
      fireInputEvent: true,
    });
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true }, win);
    Assert.ok(UrlbarTestUtils.getOneOffSearchButtons(win).selectedButton);
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    await promise;
    await SpecialPowers.popPrefEnv();
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "3",
        numWords: "1",
        selIndex: "0",
        selType: "oneoff",
      },
    };
  },

  async function(win) {
    info(
      "Type something, select one-off with enter, and select result with enter."
    );
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.urlbar.update2", true],
        ["browser.urlbar.update2.oneOffsRefresh", true],
      ],
    });
    win.gURLBar.select();

    let searchPromise = UrlbarTestUtils.promiseSearchComplete(win);

    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "test",
      fireInputEvent: true,
    });
    EventUtils.synthesizeKey("KEY_ArrowDown", { altKey: true }, win);
    let selectedOneOff = UrlbarTestUtils.getOneOffSearchButtons(win)
      .selectedButton;
    Assert.ok(selectedOneOff);
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    await searchPromise;

    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    await promise;
    await SpecialPowers.popPrefEnv();
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "4",
        numWords: "1",
        selIndex: "0",
        selType: "search",
      },
    };
  },

  async function(win) {
    info("Type something, ESC, type something else, press Enter.");
    win.gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    EventUtils.synthesizeKey("x", {}, win);
    EventUtils.synthesizeKey("VK_ESCAPE", {}, win);
    EventUtils.synthesizeKey("y", {}, win);
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "1",
        numWords: "1",
        selIndex: "0",
        selType: "search",
      },
    };
  },

  async function(win) {
    info("Type a keyword, Enter.");
    win.gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "kw test",
      fireInputEvent: true,
    });
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "7",
        numWords: "2",
        selIndex: "0",
        selType: "keyword",
      },
    };
  },

  async function(win) {
    let tipProvider = registerTipProvider();
    info("Selecting a tip's main button, enter.");
    win.gURLBar.search("x");
    await UrlbarTestUtils.promiseSearchComplete(win);
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    unregisterTipProvider(tipProvider);
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "1",
        numWords: "1",
        selIndex: "1",
        selType: "tip",
      },
    };
  },

  async function(win) {
    let tipProvider = registerTipProvider();
    info("Selecting a tip's help button, enter.");
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    win.gURLBar.search("x");
    await UrlbarTestUtils.promiseSearchComplete(win);
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
    EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
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
        numWords: "1",
        selIndex: "1",
        selType: "tiphelp",
      },
    };
  },

  async function(win) {
    info("Type something and canonize");
    win.gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "example",
      fireInputEvent: true,
    });
    EventUtils.synthesizeKey("VK_RETURN", { ctrlKey: true }, win);
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "7",
        numWords: "1",
        selIndex: "0",
        selType: "canonized",
      },
    };
  },

  async function(win) {
    info("Type something, click on bookmark entry.");
    win.gURLBar.select();
    let url = "http://example.com/?q=%s";
    let promise = BrowserTestUtils.browserLoaded(
      win.gBrowser.selectedBrowser,
      false,
      url
    );
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "exa",
      fireInputEvent: true,
    });
    while (win.gURLBar.untrimmedValue != url) {
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
    }
    let element = UrlbarTestUtils.getSelectedRow(win);
    EventUtils.synthesizeMouseAtCenter(element, {}, win);
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "click",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "3",
        numWords: "1",
        selIndex: val => parseInt(val) > 0,
        selType: "bookmark",
      },
    };
  },

  async function(win) {
    info("Type an autofilled string, Enter.");
    win.gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "exa",
      fireInputEvent: true,
    });
    // Check it's autofilled.
    Assert.equal(win.gURLBar.selectionStart, 3);
    Assert.equal(win.gURLBar.selectionEnd, 12);
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "3",
        numWords: "1",
        selIndex: "0",
        selType: "autofill",
      },
    };
  },

  async function(win) {
    info("Type something, select bookmark entry, Enter.");
    win.gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "exa",
      fireInputEvent: true,
    });
    while (win.gURLBar.untrimmedValue != "http://example.com/?q=%s") {
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
    }
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "3",
        numWords: "1",
        selIndex: val => parseInt(val) > 0,
        selType: "bookmark",
      },
    };
  },

  async function(win) {
    info("Type something, select remote search suggestion, Enter.");
    win.gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "foo",
      fireInputEvent: true,
    });
    while (win.gURLBar.untrimmedValue != "foofoo") {
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
    }
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "3",
        numWords: "1",
        selIndex: val => parseInt(val) > 0,
        selType: "searchsuggestion",
      },
    };
  },

  async function(win) {
    info("Type something, select form history, Enter.");
    await SpecialPowers.pushPrefEnv({
      set: [["browser.urlbar.maxHistoricalSearchSuggestions", 2]],
    });
    await UrlbarTestUtils.formHistory.add(["foofoo", "foobar"]);
    win.gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "foo",
      fireInputEvent: true,
    });
    while (win.gURLBar.untrimmedValue != "foofoo") {
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
    }
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "3",
        numWords: "1",
        selIndex: val => parseInt(val) > 0,
        selType: "formhistory",
      },
    };
  },

  async function(win) {
    info("Type @, Enter on a keywordoffer. Update 2 disabled.");
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.urlbar.update2", false],
        ["browser.urlbar.update2.oneOffsRefresh", false],
      ],
    });
    win.gURLBar.select();
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "@",
      fireInputEvent: true,
    });
    while (win.gURLBar.untrimmedValue != "@test ") {
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
    }
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    await UrlbarTestUtils.promiseSearchComplete(win);
    await SpecialPowers.popPrefEnv();
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "1",
        numWords: "1",
        selIndex: val => parseInt(val) > 0,
        selType: "keywordoffer",
      },
    };
  },

  async function(win) {
    info("Type @, enter on a keywordoffer, then search and press enter.");
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.urlbar.update2", true],
        ["browser.urlbar.update2.oneOffsRefresh", true],
      ],
    });
    win.gURLBar.select();
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "@",
      fireInputEvent: true,
    });

    while (win.gURLBar.searchMode?.engineName != "Test") {
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
    }
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    await UrlbarTestUtils.promiseSearchComplete(win);

    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "moz",
      fireInputEvent: true,
    });
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    await promise;

    await SpecialPowers.popPrefEnv();
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "3",
        numWords: "1",
        selIndex: "0",
        selType: "search",
      },
    };
  },

  async function(win) {
    info("Type an @alias, then space, then search and press enter.");
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.urlbar.update2", true],
        ["browser.urlbar.update2.oneOffsRefresh", true],
      ],
    });

    const alias = "testalias";
    let aliasEngine = await Services.search.addEngineWithDetails("AliasTest", {
      alias,
      template: "http://example.com/?search={searchTerms}",
    });

    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: `${alias} `,
    });

    await UrlbarTestUtils.assertSearchMode(window, {
      engineName: "AliasTest",
      entry: "typed",
    });

    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "moz",
      fireInputEvent: true,
    });
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    await promise;

    await Services.search.removeEngine(aliasEngine);
    await SpecialPowers.popPrefEnv();
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "3",
        numWords: "1",
        selIndex: "0",
        selType: "search",
      },
    };
  },

  async function(win) {
    info("Drop something.");
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    EventUtils.synthesizeDrop(
      win.document.getElementById("home-button"),
      win.gURLBar.inputField,
      [[{ type: "text/plain", data: "www.example.com" }]],
      "copy",
      win
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
        numWords: "1",
        selIndex: "-1",
        selType: "none",
      },
    };
  },

  async function(win) {
    info("Paste & Go something.");
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    await SimpleTest.promiseClipboardChange("www.example.com", () => {
      clipboardHelper.copyString("www.example.com");
    });
    let inputBox = win.gURLBar.querySelector("moz-input-box");
    let cxmenu = inputBox.menupopup;
    let cxmenuPromise = BrowserTestUtils.waitForEvent(cxmenu, "popupshown");
    EventUtils.synthesizeMouseAtCenter(
      win.gURLBar.inputField,
      {
        type: "contextmenu",
        button: 2,
      },
      win
    );
    await cxmenuPromise;
    let menuitem = inputBox.getMenuItem("paste-and-go");
    EventUtils.synthesizeMouseAtCenter(menuitem, {}, win);
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "paste_go",
      value: "pasted",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "15",
        numWords: "1",
        selIndex: "-1",
        selType: "none",
      },
    };
  },

  // The URLs in the down arrow/autoOpen tests must vary from test to test,
  // else  the first Top Site results will be a switch-to-tab result and a page
  // load will not occur.
  async function(win) {
    info("Open the panel with DOWN, select with DOWN, Enter.");
    await addTopSite("http://example.org/");
    win.gURLBar.value = "";
    win.gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    await UrlbarTestUtils.promisePopupOpen(win, () => {
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
    });
    await UrlbarTestUtils.promiseSearchComplete(win);
    while (win.gURLBar.untrimmedValue != "http://example.org/") {
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
    }
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "topsites",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "0",
        numWords: "0",
        selType: "history",
        selIndex: val => parseInt(val) >= 0,
      },
    };
  },

  async function(win) {
    info("Open the panel with DOWN, click on entry.");
    await addTopSite("http://example.com/");
    win.gURLBar.value = "";
    win.gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    await UrlbarTestUtils.promisePopupOpen(win, () => {
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
    });
    while (win.gURLBar.untrimmedValue != "http://example.com/") {
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
    }
    let element = UrlbarTestUtils.getSelectedRow(win);
    EventUtils.synthesizeMouseAtCenter(element, {}, win);
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "click",
      value: "topsites",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "0",
        numWords: "0",
        selType: "history",
        selIndex: "0",
      },
    };
  },

  // The URLs in the autoOpen tests must vary from test to test, else
  // the first Top Site results will be a switch-to-tab result and a page load
  // will not occur.
  async function(win) {
    info(
      "With pageproxystate=valid, autoopen the panel, select with DOWN, Enter."
    );
    await addTopSite("http://example.org/");
    win.gURLBar.value = "";
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    await UrlbarTestUtils.promisePopupOpen(win, () => {
      win.document.getElementById("Browser:OpenLocation").doCommand();
    });
    await SpecialPowers.popPrefEnv();
    await UrlbarTestUtils.promiseSearchComplete(win);
    while (win.gURLBar.untrimmedValue != "http://example.org/") {
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
    }
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "topsites",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "0",
        numWords: "0",
        selType: "history",
        selIndex: val => parseInt(val) >= 0,
      },
    };
  },

  async function(win) {
    info("With pageproxystate=valid, autoopen the panel, click on entry.");
    await addTopSite("http://example.com/");
    win.gURLBar.value = "";
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    await UrlbarTestUtils.promisePopupOpen(win, () => {
      win.document.getElementById("Browser:OpenLocation").doCommand();
    });
    await SpecialPowers.popPrefEnv();
    await UrlbarTestUtils.promiseSearchComplete(win);
    while (win.gURLBar.untrimmedValue != "http://example.com/") {
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
    }
    let element = UrlbarTestUtils.getSelectedRow(win);
    EventUtils.synthesizeMouseAtCenter(element, {}, win);
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "click",
      value: "topsites",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "0",
        numWords: "0",
        selType: "history",
        selIndex: "0",
      },
    };
  },

  async function(win) {
    info("With pageproxystate=invalid, open retained results, Enter.");
    await addTopSite("http://example.org/");
    win.gURLBar.value = "example.org";
    win.gURLBar.setPageProxyState("invalid");
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    await UrlbarTestUtils.promisePopupOpen(win, () => {
      win.document.getElementById("Browser:OpenLocation").doCommand();
    });
    await UrlbarTestUtils.promiseSearchComplete(win);
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "returned",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "11",
        numWords: "1",
        selType: "autofill",
        selIndex: "0",
      },
    };
  },

  async function(win) {
    info("With pageproxystate=invalid, open retained results, click on entry.");
    // This value must be different from the previous test, to avoid reopening
    // the view.
    win.gURLBar.value = "example.com";
    win.gURLBar.setPageProxyState("invalid");
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    await UrlbarTestUtils.promisePopupOpen(win, () => {
      win.document.getElementById("Browser:OpenLocation").doCommand();
    });
    await UrlbarTestUtils.promiseSearchComplete(win);
    let element = UrlbarTestUtils.getSelectedRow(win);
    EventUtils.synthesizeMouseAtCenter(element, {}, win);
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "click",
      value: "returned",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "11",
        numWords: "1",
        selType: "autofill",
        selIndex: "0",
      },
    };
  },

  async function(win) {
    info("Reopen the view: type, blur, focus, confirm.");
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "search",
      fireInputEvent: true,
    });
    await UrlbarTestUtils.promisePopupClose(win, () => {
      win.gURLBar.blur();
    });
    await UrlbarTestUtils.promisePopupOpen(win, () => {
      win.document.getElementById("Browser:OpenLocation").doCommand();
    });
    await UrlbarTestUtils.promiseSearchComplete(win);
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    await promise;
    return [
      {
        category: "urlbar",
        method: "abandonment",
        object: "blur",
        value: "typed",
        extra: {
          elapsed: val => parseInt(val) > 0,
          numChars: "6",
          numWords: "1",
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
          numWords: "1",
          selType: "search",
          selIndex: "0",
        },
      },
    ];
  },

  async function(win) {
    info("Open search mode with a keyboard shortcut.");
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.urlbar.update2", true],
        ["browser.urlbar.update2.oneOffsRefresh", true],
      ],
    });
    let defaultEngine = await Services.search.getDefault();
    win.gURLBar.select();
    EventUtils.synthesizeKey("k", { accelKey: true });
    await UrlbarTestUtils.assertSearchMode(win, {
      source: UrlbarUtils.RESULT_SOURCE.SEARCH,
      engineName: defaultEngine.name,
      entry: "shortcut",
    });

    // Execute a search to finish the engagement.
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "moz",
      fireInputEvent: true,
    });
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    await promise;

    await SpecialPowers.popPrefEnv();
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "3",
        numWords: "1",
        selIndex: "0",
        selType: "search",
      },
    };
  },

  async function(win) {
    info("Sanity check we are not stuck on 'returned'");
    win.gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "x",
      fireInputEvent: true,
    });
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "1",
        numWords: "1",
        selIndex: "0",
        selType: "search",
      },
    };
  },

  async function(win) {
    info("Reopen the view: type, blur, focus, backspace, type, confirm.");
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "search",
      fireInputEvent: true,
    });
    await UrlbarTestUtils.promisePopupClose(win, () => {
      win.gURLBar.blur();
    });
    await UrlbarTestUtils.promisePopupOpen(win, () => {
      win.document.getElementById("Browser:OpenLocation").doCommand();
    });
    EventUtils.synthesizeKey("VK_RIGHT", {}, win);
    EventUtils.synthesizeKey("VK_BACK_SPACE", {}, win);
    EventUtils.synthesizeKey("x", {}, win);
    await UrlbarTestUtils.promiseSearchComplete(win);
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    await promise;
    return [
      {
        category: "urlbar",
        method: "abandonment",
        object: "blur",
        value: "typed",
        extra: {
          elapsed: val => parseInt(val) > 0,
          numChars: "6",
          numWords: "1",
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
          numWords: "1",
          selType: "search",
          selIndex: "0",
        },
      },
    ];
  },

  async function(win) {
    info("Reopen the view: type, blur, focus, type (overwrite), confirm.");
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "search",
      fireInputEvent: true,
    });
    await UrlbarTestUtils.promisePopupClose(win, () => {
      win.gURLBar.blur();
    });
    await UrlbarTestUtils.promisePopupOpen(win, () => {
      win.document.getElementById("Browser:OpenLocation").doCommand();
    });
    EventUtils.synthesizeKey("x", {}, win);
    await UrlbarTestUtils.promiseSearchComplete(win);
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    await promise;
    return [
      {
        category: "urlbar",
        method: "abandonment",
        object: "blur",
        value: "typed",
        extra: {
          elapsed: val => parseInt(val) > 0,
          numChars: "6",
          numWords: "1",
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
          numWords: "1",
          selType: "search",
          selIndex: "0",
        },
      },
    ];
  },

  async function(win) {
    info("Sanity check we are not stuck on 'restarted'");
    win.gURLBar.select();
    let promise = BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window: win,
      value: "x",
      fireInputEvent: true,
    });
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
    await promise;
    return {
      category: "urlbar",
      method: "engagement",
      object: "enter",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "1",
        numWords: "1",
        selIndex: "0",
        selType: "search",
      },
    };
  },

  /*
   * Abandonment tests.
   */

  async function(win) {
    info("Type something, blur.");
    win.gURLBar.select();
    EventUtils.synthesizeKey("x", {}, win);
    win.gURLBar.blur();
    return {
      category: "urlbar",
      method: "abandonment",
      object: "blur",
      value: "typed",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "1",
        numWords: "1",
      },
    };
  },

  async function(win) {
    info("Open the panel with DOWN, don't type, blur it.");
    win.gURLBar.value = "";
    win.gURLBar.select();
    await UrlbarTestUtils.promisePopupOpen(win, () => {
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
    });
    win.gURLBar.blur();
    return {
      category: "urlbar",
      method: "abandonment",
      object: "blur",
      value: "topsites",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "0",
        numWords: "0",
      },
    };
  },

  async function(win) {
    info("With pageproxystate=valid, autoopen the panel, don't type, blur it.");
    win.gURLBar.value = "";
    await UrlbarTestUtils.promisePopupOpen(win, () => {
      win.document.getElementById("Browser:OpenLocation").doCommand();
    });
    win.gURLBar.blur();
    return {
      category: "urlbar",
      method: "abandonment",
      object: "blur",
      value: "topsites",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "0",
        numWords: "0",
      },
    };
  },

  async function(win) {
    info("Enter search mode from Top Sites.");
    await SpecialPowers.pushPrefEnv({
      set: [
        ["browser.urlbar.update2", true],
        ["browser.urlbar.update2.oneOffsRefresh", true],
      ],
    });
    await updateTopSites(sites => true, /* enableSearchShorcuts */ true);

    win.gURLBar.value = "";
    win.gURLBar.select();
    await UrlbarTestUtils.promisePopupOpen(win, () => {
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
    });
    while (win.gURLBar.searchMode?.engineName != "Google") {
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);
    }
    let element = UrlbarTestUtils.getSelectedRow(win);
    Assert.ok(
      element.result.source == UrlbarUtils.RESULT_SOURCE.SEARCH,
      "The selected result is a search Top Site."
    );

    let engine = element.result.payload.engine;
    let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
    EventUtils.synthesizeMouseAtCenter(element, {}, win);
    await searchPromise;
    await UrlbarTestUtils.assertSearchMode(win, {
      engineName: engine,
      source: UrlbarUtils.RESULT_SOURCE.SEARCH,
      entry: "topsites_urlbar",
    });

    // To avoid needing to add a custom search shortcut Top Site, we just
    // abandon this interaction.
    win.gURLBar.blur();

    await SpecialPowers.popPrefEnv();
    return {
      category: "urlbar",
      method: "abandonment",
      object: "blur",
      value: "topsites",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "0",
        numWords: "0",
      },
    };
  },

  async function(win) {
    info(
      "With pageproxystate=invalid, open retained results, don't type, blur it."
    );
    win.gURLBar.value = "mochi.test";
    win.gURLBar.setPageProxyState("invalid");
    await UrlbarTestUtils.promisePopupOpen(win, () => {
      win.document.getElementById("Browser:OpenLocation").doCommand();
    });
    win.gURLBar.blur();
    return {
      category: "urlbar",
      method: "abandonment",
      object: "blur",
      value: "returned",
      extra: {
        elapsed: val => parseInt(val) > 0,
        numChars: "10",
        numWords: "1",
      },
    };
  },
];

const noEventTests = [
  async function(win) {
    info("Type something, click on search settings.");
    await BrowserTestUtils.withNewTab(
      { gBrowser: win.gBrowser, url: "about:blank" },
      async browser => {
        win.gURLBar.select();
        let promise = BrowserTestUtils.browserLoaded(browser);
        await UrlbarTestUtils.promiseAutocompleteResultPopup({
          window: win,
          value: "x",
          fireInputEvent: true,
        });
        UrlbarTestUtils.getOneOffSearchButtons(win).settingsButton.click();
        await promise;
      }
    );
    return null;
  },

  async function(win) {
    info("Type something, Up, Enter on search settings.");
    await BrowserTestUtils.withNewTab(
      { gBrowser: win.gBrowser, url: "about:blank" },
      async browser => {
        win.gURLBar.select();
        let promise = BrowserTestUtils.browserLoaded(browser);
        await UrlbarTestUtils.promiseAutocompleteResultPopup({
          window: win,
          value: "x",
          fireInputEvent: true,
        });
        EventUtils.synthesizeKey("KEY_ArrowUp", {}, win);
        Assert.ok(
          UrlbarTestUtils.getOneOffSearchButtons(
            win
          ).selectedButton.classList.contains("search-setting-button-compact"),
          "Should have selected the settings button"
        );
        EventUtils.synthesizeKey("VK_RETURN", {}, win);
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
    await UrlbarTestUtils.formHistory.clear(window);
  });

  // This is not necessary after each loop, because assertEvents does it.
  Services.telemetry.clearEvents();

  for (let i = 0; i < tests.length; i++) {
    info(`Running test at index ${i}`);
    let events = await tests[i](window);
    if (!Array.isArray(events)) {
      events = [events];
    }
    // Always blur to ensure it's not accounted as an additional abandonment.
    window.gURLBar.setSearchMode({});
    gURLBar.blur();
    TelemetryTestUtils.assertEvents(events, { category: "urlbar" });
    await UrlbarTestUtils.formHistory.clear(window);
  }

  for (let i = 0; i < noEventTests.length; i++) {
    info(`Running no event test at index ${i}`);
    await noEventTests[i](window);
    // Always blur to ensure it's not accounted as an additional abandonment.
    gURLBar.blur();
    TelemetryTestUtils.assertEvents([], { category: "urlbar" });
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
  let provider = new UrlbarTestUtils.TestProvider({
    results: tipMatches,
    priority: 1,
  });
  UrlbarProvidersManager.registerProvider(provider);
  return provider;
}

function unregisterTipProvider(provider) {
  UrlbarProvidersManager.unregisterProvider(provider);
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
      type: "test",
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
