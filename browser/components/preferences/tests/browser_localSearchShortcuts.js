/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Checks the local shortcut rows in the engines list of the search pane.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

let gTree;

add_task(async function init() {
  let prefs = await openPreferencesViaOpenPreferencesAPI("search", {
    leaveOpen: true,
  });
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });

  Assert.equal(
    prefs.selectedPane,
    "paneSearch",
    "Sanity check: Search pane is selected by default"
  );

  gTree = gBrowser.contentDocument.querySelector("#engineList");
  gTree.scrollIntoView();
  gTree.focus();
});

// The rows should be visible and checked by default.
add_task(async function visible() {
  await checkRowVisibility(true);
  await forEachLocalShortcutRow(async (row, shortcut) => {
    Assert.equal(
      gTree.view.getCellValue(row, gTree.columns.getNamedColumn("engineShown")),
      "true",
      "Row is checked initially"
    );
  });
});

// Toggling the browser.urlbar.shortcuts.* prefs should toggle the corresponding
// checkboxes in the rows.
add_task(async function syncFromPrefs() {
  let col = gTree.columns.getNamedColumn("engineShown");
  await forEachLocalShortcutRow(async (row, shortcut) => {
    Assert.equal(
      gTree.view.getCellValue(row, col),
      "true",
      "Row is checked initially"
    );
    await SpecialPowers.pushPrefEnv({
      set: [[getUrlbarPrefName(shortcut.pref), false]],
    });
    Assert.equal(
      gTree.view.getCellValue(row, col),
      "false",
      "Row is unchecked after disabling pref"
    );
    await SpecialPowers.popPrefEnv();
    Assert.equal(
      gTree.view.getCellValue(row, col),
      "true",
      "Row is checked after re-enabling pref"
    );
  });
});

// Pressing the space key while a row is selected should toggle its checkbox
// and pref.
add_task(async function syncToPrefs_spaceKey() {
  let col = gTree.columns.getNamedColumn("engineShown");
  await forEachLocalShortcutRow(async (row, shortcut) => {
    Assert.ok(
      UrlbarPrefs.get(shortcut.pref),
      "Sanity check: Pref is enabled initially"
    );
    Assert.equal(
      gTree.view.getCellValue(row, col),
      "true",
      "Row is checked initially"
    );
    gTree.view.selection.select(row);
    EventUtils.synthesizeKey(" ", {}, gTree.ownerGlobal);
    Assert.ok(
      !UrlbarPrefs.get(shortcut.pref),
      "Pref is disabled after pressing space key"
    );
    Assert.equal(
      gTree.view.getCellValue(row, col),
      "false",
      "Row is unchecked after pressing space key"
    );
    Services.prefs.clearUserPref(getUrlbarPrefName(shortcut.pref));
  });
});

// Clicking the checkbox in a local shortcut row should toggle the checkbox and
// pref.
add_task(async function syncToPrefs_click() {
  let col = gTree.columns.getNamedColumn("engineShown");
  await forEachLocalShortcutRow(async (row, shortcut) => {
    Assert.ok(
      UrlbarPrefs.get(shortcut.pref),
      "Sanity check: Pref is enabled initially"
    );
    Assert.equal(
      gTree.view.getCellValue(row, col),
      "true",
      "Row is checked initially"
    );

    let rect = gTree.getCoordsForCellItem(row, col, "cell");
    let x = rect.x + rect.width / 2;
    let y = rect.y + rect.height / 2;
    EventUtils.synthesizeMouse(gTree.body, x, y, {}, gTree.ownerGlobal);

    Assert.ok(
      !UrlbarPrefs.get(shortcut.pref),
      "Pref is disabled after clicking checkbox"
    );
    Assert.equal(
      gTree.view.getCellValue(row, col),
      "false",
      "Row is unchecked after clicking checkbox"
    );
    Services.prefs.clearUserPref(getUrlbarPrefName(shortcut.pref));
  });
});

// The keyword column should not be editable according to isEditable().
add_task(async function keywordNotEditable_isEditable() {
  await forEachLocalShortcutRow(async (row, shortcut) => {
    Assert.ok(
      !gTree.view.isEditable(
        row,
        gTree.columns.getNamedColumn("engineKeyword")
      ),
      "Keyword column is not editable"
    );
  });
});

// Pressing the enter key while a row is selected shouldn't allow the keyword to
// be edited.
add_task(async function keywordNotEditable_enterKey() {
  let col = gTree.columns.getNamedColumn("engineKeyword");
  await forEachLocalShortcutRow(async (row, shortcut) => {
    Assert.ok(
      shortcut.restrict,
      "Sanity check: Shortcut restriction char is non-empty"
    );
    Assert.equal(
      gTree.view.getCellText(row, col),
      shortcut.restrict,
      "Sanity check: Keyword column has correct restriction char initially"
    );

    gTree.view.selection.select(row);
    EventUtils.synthesizeKey("KEY_Enter", {}, gTree.ownerGlobal);
    EventUtils.sendString("newkeyword");
    EventUtils.synthesizeKey("KEY_Enter", {}, gTree.ownerGlobal);

    // Wait a moment to allow for any possible asynchronicity.
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(r => setTimeout(r, 500));

    Assert.equal(
      gTree.view.getCellText(row, col),
      shortcut.restrict,
      "Keyword column is still restriction char"
    );
  });
});

// Double-clicking the keyword column shouldn't allow the keyword to be edited.
add_task(async function keywordNotEditable_click() {
  let col = gTree.columns.getNamedColumn("engineKeyword");
  await forEachLocalShortcutRow(async (row, shortcut) => {
    Assert.ok(
      shortcut.restrict,
      "Sanity check: Shortcut restriction char is non-empty"
    );
    Assert.equal(
      gTree.view.getCellText(row, col),
      shortcut.restrict,
      "Sanity check: Keyword column has correct restriction char initially"
    );

    let rect = gTree.getCoordsForCellItem(row, col, "text");
    let x = rect.x + rect.width / 2;
    let y = rect.y + rect.height / 2;

    let promise = BrowserTestUtils.waitForEvent(gTree, "dblclick");

    // Click once to select the row.
    EventUtils.synthesizeMouse(
      gTree.body,
      x,
      y,
      { clickCount: 1 },
      gTree.ownerGlobal
    );

    // Now double-click the keyword column.
    EventUtils.synthesizeMouse(
      gTree.body,
      x,
      y,
      { clickCount: 2 },
      gTree.ownerGlobal
    );

    await promise;

    EventUtils.sendString("newkeyword");
    EventUtils.synthesizeKey("KEY_Enter", {}, gTree.ownerGlobal);

    // Wait a moment to allow for any possible asynchronicity.
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(r => setTimeout(r, 500));

    Assert.equal(
      gTree.view.getCellText(row, col),
      shortcut.restrict,
      "Keyword column is still restriction char"
    );
  });
});

/**
 * Asserts that the engine and local shortcut rows are present in the tree.
 */
async function checkRowVisibility() {
  let engines = await Services.search.getVisibleEngines();

  Assert.equal(
    gTree.view.rowCount,
    engines.length + UrlbarUtils.LOCAL_SEARCH_MODES.length,
    "Expected number of tree rows"
  );

  // Check the engine rows.
  for (let row = 0; row < engines.length; row++) {
    let engine = engines[row];
    let text = gTree.view.getCellText(
      row,
      gTree.columns.getNamedColumn("engineName")
    );
    Assert.equal(
      text,
      engine.name,
      `Sanity check: Tree row ${row} has expected engine name`
    );
  }

  // Check the shortcut rows.
  await forEachLocalShortcutRow(async (row, shortcut) => {
    let text = gTree.view.getCellText(
      row,
      gTree.columns.getNamedColumn("engineName")
    );
    let name = UrlbarUtils.getResultSourceName(shortcut.source);
    let l10nName = await gTree.ownerDocument.l10n.formatValue(
      `urlbar-search-mode-${name}`
    );
    Assert.ok(l10nName, "Sanity check: l10n name is non-empty");
    Assert.equal(text, l10nName, `Tree row ${row} has expected shortcut name`);
  });
}

/**
 * Calls a callback for each local shortcut row in the tree.
 *
 * @param {function} callback
 *   Called for each local shortcut row like: callback(rowIndex, shortcutObject)
 */
async function forEachLocalShortcutRow(callback) {
  let engines = await Services.search.getVisibleEngines();
  for (let i = 0; i < UrlbarUtils.LOCAL_SEARCH_MODES.length; i++) {
    let shortcut = UrlbarUtils.LOCAL_SEARCH_MODES[i];
    let row = engines.length + i;
    // These tests assume LOCAL_SEARCH_MODES are enabled, this can be removed
    // when we enable QuickActions. We cant just enable the pref in browser.ini
    // as this test calls clearUserPref.
    if (shortcut.pref == "shortcuts.quickactions") {
      continue;
    }
    await callback(row, shortcut);
  }
}

/**
 * Prepends the `browser.urlbar.` branch to the given relative pref.
 *
 * @param {string} relativePref
 *   A pref name relative to the `browser.urlbar.`.
 * @returns {string}
 *   The full pref name with `browser.urlbar.` prepended.
 */
function getUrlbarPrefName(relativePref) {
  return `browser.urlbar.${relativePref}`;
}
