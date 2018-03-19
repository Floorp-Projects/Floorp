/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const NON_EMPTY_TAB = "example.com/non-empty";
const EMPTY_TAB = "about:blank";
const META_KEY = AppConstants.platform == "macosx" ? "metaKey" : "ctrlKey";
const ENTER = new KeyboardEvent("keydown", {});
const ALT_ENTER = new KeyboardEvent("keydown", {altKey: true});
const CLICK = new MouseEvent("click", {button: 0});
const META_CLICK = new MouseEvent("click", {button: 0, [META_KEY]: true});
const MIDDLE_CLICK = new MouseEvent("click", {button: 1});


let old_openintab = Preferences.get("browser.urlbar.openintab");
registerCleanupFunction(async function() {
  Preferences.set("browser.urlbar.openintab", old_openintab);
});

add_task(async function openInTab() {
  // Open a non-empty tab.
  let tab = gBrowser.selectedTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, NON_EMPTY_TAB);

  for (let test of [{pref: false, event: ALT_ENTER,
                     desc: "Alt+Enter, non-empty tab, default prefs"},
                    {pref: false, event: META_CLICK,
                     desc: "Meta+click, non-empty tab, default prefs"},
                    {pref: false, event: MIDDLE_CLICK,
                     desc: "Middle click, non-empty tab, default prefs"},
                    {pref: true, event: ENTER,
                     desc: "Enter, non-empty tab, openInTab"},
                    {pref: true, event: CLICK,
                     desc: "Normal click, non-empty tab, openInTab"}]) {
    info(test.desc);

    Preferences.set("browser.urlbar.openintab", test.pref);
    let where = gURLBar._whereToOpen(test.event);
    is(where, "tab", "URL would be loaded in a new tab");
  }

  // Clean up.
  BrowserTestUtils.removeTab(tab);
});

add_task(async function keepEmptyTab() {
  // Open an empty tab.
  let tab = gBrowser.selectedTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, EMPTY_TAB);

  for (let test of [{pref: false, event: META_CLICK,
                     desc: "Meta+click, empty tab, default prefs"},
                    {pref: false, event: MIDDLE_CLICK,
                     desc: "Middle click, empty tab, default prefs"}]) {
    info(test.desc);

    Preferences.set("browser.urlbar.openintab", test.pref);
    let where = gURLBar._whereToOpen(test.event);
    is(where, "tab", "URL would be loaded in a new tab");
  }

  // Clean up.
  BrowserTestUtils.removeTab(tab);
});

add_task(async function reuseEmptyTab() {
  // Open an empty tab.
  let tab = gBrowser.selectedTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, EMPTY_TAB);

  for (let test of [{pref: false, event: ALT_ENTER,
                     desc: "Alt+Enter, empty tab, default prefs"},
                    {pref: true, event: ENTER,
                     desc: "Enter, empty tab, openInTab"},
                    {pref: true, event: CLICK,
                     desc: "Normal click, empty tab, openInTab"}]) {
    info(test.desc);
    Preferences.set("browser.urlbar.openintab", test.pref);
    let where = gURLBar._whereToOpen(test.event);
    is(where, "current", "New URL would reuse the current empty tab");
  }

  // Clean up.
  BrowserTestUtils.removeTab(tab);
});

add_task(async function openInCurrentTab() {
  for (let test of [{pref: false, url: NON_EMPTY_TAB, event: ENTER,
                     desc: "Enter, non-empty tab, default prefs"},
                    {pref: false, url: NON_EMPTY_TAB, event: CLICK,
                     desc: "Normal click, non-empty tab, default prefs"},
                    {pref: false, url: EMPTY_TAB, event: ENTER,
                     desc: "Enter, empty tab, default prefs"},
                    {pref: false, url: EMPTY_TAB, event: CLICK,
                     desc: "Normal click, empty tab, default prefs"},
                    {pref: true, url: NON_EMPTY_TAB, event: ALT_ENTER,
                     desc: "Alt+Enter, non-empty tab, openInTab"},
                    {pref: true, url: NON_EMPTY_TAB, event: META_CLICK,
                     desc: "Meta+click, non-empty tab, openInTab"},
                    {pref: true, url: NON_EMPTY_TAB, event: MIDDLE_CLICK,
                     desc: "Middle click, non-empty tab, openInTab"}]) {
    info(test.desc);

    // Open a new tab.
    let tab = gBrowser.selectedTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, test.url);

    Preferences.set("browser.urlbar.openintab", test.pref);
    let where = gURLBar._whereToOpen(test.event);
    is(where, "current", "URL would open in the current tab");

    // Clean up.
    BrowserTestUtils.removeTab(tab);
  }
});
