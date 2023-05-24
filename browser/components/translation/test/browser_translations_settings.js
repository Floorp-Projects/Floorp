/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// tests the translation infobar, using a fake 'Translation' implementation.

const { PermissionTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PermissionTestUtils.sys.mjs"
);

const TRANSLATIONS_PERMISSION = "translations";
const ENABLE_TRANSLATIONS_PREF = "browser.translations.enable";
const ALWAYS_TRANSLATE_LANGS_PREF =
  "browser.translations.alwaysTranslateLanguages";
const NEVER_TRANSLATE_LANGS_PREF =
  "browser.translations.neverTranslateLanguages";

function test() {
  waitForExplicitFinish();
  Services.prefs.setBoolPref(ENABLE_TRANSLATIONS_PREF, true);
  Services.prefs.setCharPref(ALWAYS_TRANSLATE_LANGS_PREF, "");
  Services.prefs.setCharPref(NEVER_TRANSLATE_LANGS_PREF, "");
  let tab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = tab;
  registerCleanupFunction(function () {
    gBrowser.removeTab(tab);
    Services.prefs.clearUserPref(ENABLE_TRANSLATIONS_PREF);
    Services.prefs.clearUserPref(ALWAYS_TRANSLATE_LANGS_PREF);
    Services.prefs.clearUserPref(NEVER_TRANSLATE_LANGS_PREF);
  });
  BrowserTestUtils.browserLoaded(tab.linkedBrowser).then(() => {
    (async function () {
      for (let testCase of gTests) {
        info(testCase.desc);
        await testCase.run();
      }
    })().then(finish, ex => {
      ok(false, "Unexpected Exception: " + ex);
      finish();
    });
  });

  BrowserTestUtils.loadURIString(
    gBrowser.selectedBrowser,
    "https://example.com/"
  );
}

/**
 * Retrieves the always-translate language list as an array.
 * @returns {Array<string>}
 */
function getAlwaysTranslateLanguages() {
  let langs = Services.prefs.getCharPref(ALWAYS_TRANSLATE_LANGS_PREF);
  return langs ? langs.split(",") : [];
}

/**
 * Retrieves the always-translate language list as an array.
 * @returns {Array<string>}
 */
function getNeverTranslateLanguages() {
  let langs = Services.prefs.getCharPref(NEVER_TRANSLATE_LANGS_PREF);
  return langs ? langs.split(",") : [];
}

/**
 * Retrieves the always-translate site list as an array.
 * @returns {Array<string>}
 */
function getNeverTranslateSites() {
  let results = [];
  for (let perm of Services.perms.all) {
    if (
      perm.type == TRANSLATIONS_PERMISSION &&
      perm.capability == Services.perms.DENY_ACTION
    ) {
      results.push(perm.principal);
    }
  }

  return results;
}

function openPopup(aPopup) {
  return new Promise(resolve => {
    aPopup.addEventListener(
      "popupshown",
      function () {
        TestUtils.executeSoon(resolve);
      },
      { once: true }
    );

    aPopup.focus();
    // One down event to open the popup.
    EventUtils.synthesizeKey("VK_DOWN");
  });
}

function waitForWindowLoad(aWin) {
  return new Promise(resolve => {
    aWin.addEventListener(
      "load",
      function () {
        TestUtils.executeSoon(resolve);
      },
      { capture: true, once: true }
    );
  });
}

var gTests = [
  {
    desc: "ensure lists are empty on startup",
    run: function checkPreferencesAreEmpty() {
      is(
        getAlwaysTranslateLanguages().length,
        0,
        "we start with an empty list of languages to always translate"
      );
      is(
        getNeverTranslateLanguages().length,
        0,
        "we start with an empty list of languages to never translate"
      );
      is(
        getNeverTranslateSites().length,
        0,
        "we start with an empty list of sites to never translate"
      );
    },
  },

  {
    desc: "ensure always-translate languages function correctly",
    run: async function testAlwaysTranslateLanguages() {
      // Put 2 languages in the pref before opening the window to check
      // the list is displayed on load.
      Services.prefs.setCharPref(ALWAYS_TRANSLATE_LANGS_PREF, "fr,de");

      // Open the translations settings dialog.
      let win = openDialog(
        "chrome://browser/content/preferences/dialogs/translations.xhtml",
        "Browser:TranslationsPreferences",
        "",
        null
      );
      await waitForWindowLoad(win);

      // Check that the list of always-translate languages is loaded.
      let getById = win.document.getElementById.bind(win.document);
      let tree = getById("alwaysTranslateLanguagesTree");
      let remove = getById("removeAlwaysTranslateLanguage");
      let removeAll = getById("removeAllAlwaysTranslateLanguages");
      is(
        tree.view.rowCount,
        2,
        "The always-translate languages list has 2 items"
      );
      ok(remove.disabled, "The 'Remove Language' button is disabled");
      ok(!removeAll.disabled, "The 'Remove All Languages' button is enabled");

      // Select the first item.
      tree.view.selection.select(0);
      ok(!remove.disabled, "The 'Remove Language' button is enabled");

      // Click the 'Remove' button.
      remove.click();
      is(
        tree.view.rowCount,
        1,
        "The always-translate languages list now contains 1 item"
      );
      is(
        getAlwaysTranslateLanguages().length,
        1,
        "One language tag in the pref"
      );

      // Clear the pref, and check the last item is removed from the display.
      Services.prefs.setCharPref(ALWAYS_TRANSLATE_LANGS_PREF, "");
      is(tree.view.rowCount, 0, "The always-translate languages list is empty");
      ok(remove.disabled, "The 'Remove Language' button is disabled");
      ok(removeAll.disabled, "The 'Remove All Languages' button is disabled");

      // Add an item and check it appears.
      Services.prefs.setCharPref(ALWAYS_TRANSLATE_LANGS_PREF, "fr,en,es");
      is(
        tree.view.rowCount,
        3,
        "The always-translate languages list has 3 items"
      );
      ok(remove.disabled, "The 'Remove Language' button is disabled");
      ok(!removeAll.disabled, "The 'Remove All Languages' button is enabled");

      // Click the 'Remove All' button.
      removeAll.click();
      is(tree.view.rowCount, 0, "The always-translate languages list is empty");
      ok(remove.disabled, "The 'Remove Language' button is disabled");
      ok(removeAll.disabled, "The 'Remove All Languages' button is disabled");
      is(
        Services.prefs.getCharPref(ALWAYS_TRANSLATE_LANGS_PREF),
        "",
        "The pref is empty"
      );

      win.close();
    },
  },

  {
    desc: "ensure never-translate languages function correctly",
    run: async function testNeverTranslateLanguages() {
      // Put 2 languages in the pref before opening the window to check
      // the list is displayed on load.
      Services.prefs.setCharPref(NEVER_TRANSLATE_LANGS_PREF, "fr,de");

      // Open the translations settings dialog.
      let win = openDialog(
        "chrome://browser/content/preferences/dialogs/translations.xhtml",
        "Browser:TranslationsPreferences",
        "",
        null
      );
      await waitForWindowLoad(win);

      // Check that the list of never-translate languages is loaded.
      let getById = win.document.getElementById.bind(win.document);
      let tree = getById("neverTranslateLanguagesTree");
      let remove = getById("removeNeverTranslateLanguage");
      let removeAll = getById("removeAllNeverTranslateLanguages");
      is(
        tree.view.rowCount,
        2,
        "The never-translate languages list has 2 items"
      );
      ok(remove.disabled, "The 'Remove Language' button is disabled");
      ok(!removeAll.disabled, "The 'Remove All Languages' button is enabled");

      // Select the first item.
      tree.view.selection.select(0);
      ok(!remove.disabled, "The 'Remove Language' button is enabled");

      // Click the 'Remove' button.
      remove.click();
      is(
        tree.view.rowCount,
        1,
        "The never-translate language list now contains 1 item"
      );
      is(getNeverTranslateLanguages().length, 1, "One langtag in the pref");

      // Clear the pref, and check the last item is removed from the display.
      Services.prefs.setCharPref(NEVER_TRANSLATE_LANGS_PREF, "");
      is(tree.view.rowCount, 0, "The never-translate languages list is empty");
      ok(remove.disabled, "The 'Remove Language' button is disabled");
      ok(removeAll.disabled, "The 'Remove All Languages' button is disabled");

      // Add an item and check it appears.
      Services.prefs.setCharPref(NEVER_TRANSLATE_LANGS_PREF, "fr,en,es");
      is(
        tree.view.rowCount,
        3,
        "The never-translate languages list has 3 items"
      );
      ok(remove.disabled, "The 'Remove Language' button is disabled");
      ok(!removeAll.disabled, "The 'Remove All Languages' button is enabled");

      // Click the 'Remove All' button.
      removeAll.click();
      is(tree.view.rowCount, 0, "The never-translate languages list is empty");
      ok(remove.disabled, "The 'Remove Language' button is disabled");
      ok(removeAll.disabled, "The 'Remove All Languages' button is disabled");
      is(
        Services.prefs.getCharPref(NEVER_TRANSLATE_LANGS_PREF),
        "",
        "The pref is empty"
      );

      win.close();
    },
  },

  {
    desc: "ensure never-translate sites function correctly",
    run: async function testNeverTranslateSites() {
      // Add two deny permissions before opening the window to
      // check the list is displayed on load.
      PermissionTestUtils.add(
        "https://example.org",
        TRANSLATIONS_PERMISSION,
        Services.perms.DENY_ACTION
      );
      PermissionTestUtils.add(
        "https://example.com",
        TRANSLATIONS_PERMISSION,
        Services.perms.DENY_ACTION
      );

      // Open the translations settings dialog.
      let win = openDialog(
        "chrome://browser/content/preferences/dialogs/translations.xhtml",
        "Browser:TranslationsPreferences",
        "",
        null
      );
      await waitForWindowLoad(win);

      // Check that the list of never-translate sites is loaded.
      let getById = win.document.getElementById.bind(win.document);
      let tree = getById("neverTranslateSitesTree");
      let remove = getById("removeNeverTranslateSite");
      let removeAll = getById("removeAllNeverTranslateSites");
      is(tree.view.rowCount, 2, "The never-translate sites list has 2 items");
      ok(remove.disabled, "The 'Remove Site' button is disabled");
      ok(!removeAll.disabled, "The 'Remove All Sites' button is enabled");

      // Select the first item.
      tree.view.selection.select(0);
      ok(!remove.disabled, "The 'Remove Site' button is enabled");

      // Click the 'Remove' button.
      remove.click();
      is(
        tree.view.rowCount,
        1,
        "The never-translate sites list now contains 1 item"
      );
      is(
        getNeverTranslateSites().length,
        1,
        "One domain in the site permissions"
      );

      // Clear the permissions, and check the last item is removed from the display.
      PermissionTestUtils.remove(
        "https://example.org",
        TRANSLATIONS_PERMISSION
      );
      PermissionTestUtils.remove(
        "https://example.com",
        TRANSLATIONS_PERMISSION
      );
      is(tree.view.rowCount, 0, "The never-translate sites list is empty");
      ok(remove.disabled, "The 'Remove Site' button is disabled");
      ok(removeAll.disabled, "The 'Remove All Site' button is disabled");

      // Add items back and check that they appear
      PermissionTestUtils.add(
        "https://example.org",
        TRANSLATIONS_PERMISSION,
        Services.perms.DENY_ACTION
      );
      PermissionTestUtils.add(
        "https://example.com",
        TRANSLATIONS_PERMISSION,
        Services.perms.DENY_ACTION
      );
      PermissionTestUtils.add(
        "https://example.net",
        TRANSLATIONS_PERMISSION,
        Services.perms.DENY_ACTION
      );

      is(tree.view.rowCount, 3, "The never-translate sites list has 3 item");
      ok(remove.disabled, "The 'Remove Site' button is disabled");
      ok(!removeAll.disabled, "The 'Remove All Sites' button is enabled");

      // Click the 'Remove All' button.
      removeAll.click();
      is(tree.view.rowCount, 0, "The never-translate sites list is empty");
      ok(remove.disabled, "The 'Remove Site' button is disabled");
      ok(removeAll.disabled, "The 'Remove All Sites' button is disabled");
      is(
        getNeverTranslateSites().length,
        0,
        "No domains in the site permissions"
      );

      win.close();
    },
  },
];
