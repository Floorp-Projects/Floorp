Services.prefs.setBoolPref("browser.preferences.instantApply", true);

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("browser.preferences.instantApply");
});

add_task(async function() {
  await openPreferencesViaOpenPreferencesAPI("general", { leaveOpen: true });
  await gBrowser.contentWindow.gMainPane._selectDefaultLanguageGroupPromise;
  await TestUtils.waitForCondition(
    () => !gBrowser.contentWindow.Preferences.updateQueued
  );

  let doc = gBrowser.contentDocument;
  let contentWindow = gBrowser.contentWindow;
  var langGroup = Services.prefs.getComplexValue(
    "font.language.group",
    Ci.nsIPrefLocalizedString
  ).data;
  is(
    contentWindow.Preferences.get("font.language.group").value,
    langGroup,
    "Language group should be set correctly."
  );

  let defaultFontType = Services.prefs.getCharPref("font.default." + langGroup);
  let fontFamilyPref = "font.name." + defaultFontType + "." + langGroup;
  let fontFamily = Services.prefs.getCharPref(fontFamilyPref);
  let fontFamilyField = doc.getElementById("defaultFont");
  is(fontFamilyField.value, fontFamily, "Font family should be set correctly.");

  function dispatchMenuItemCommand(menuItem) {
    const cmdEvent = doc.createEvent("xulcommandevent");
    cmdEvent.initCommandEvent(
      "command",
      true,
      true,
      contentWindow,
      0,
      false,
      false,
      false,
      false,
      0,
      null,
      0
    );
    menuItem.dispatchEvent(cmdEvent);
  }

  /**
   * Return a promise that resolves when the fontFamilyPref changes.
   *
   * Font prefs are the only ones whose form controls set "delayprefsave",
   * which delays the pref change when a user specifies a new value
   * for the pref.  Thus, in order to confirm that the pref gets changed
   * when the test selects a new value in a font field, we need to await
   * the change.  Awaiting this function does so for fontFamilyPref.
   */
  function fontFamilyPrefChanged() {
    return new Promise(resolve => {
      const observer = {
        observe(aSubject, aTopic, aData) {
          // Check for an exact match to avoid the ambiguity of nsIPrefBranch's
          // prefix-matching algorithm for notifying pref observers.
          if (aData == fontFamilyPref) {
            Services.prefs.removeObserver(fontFamilyPref, observer);
            resolve();
          }
        },
      };
      Services.prefs.addObserver(fontFamilyPref, observer);
    });
  }

  const menuItems = fontFamilyField.querySelectorAll("menuitem");
  ok(menuItems.length > 1, "There are multiple font menuitems.");
  ok(menuItems[0].selected, "The first (default) font menuitem is selected.");

  dispatchMenuItemCommand(menuItems[1]);
  ok(menuItems[1].selected, "The second font menuitem is selected.");

  await fontFamilyPrefChanged();
  fontFamily = Services.prefs.getCharPref(fontFamilyPref);
  is(fontFamilyField.value, fontFamily, "The font family has been updated.");

  dispatchMenuItemCommand(menuItems[0]);
  ok(
    menuItems[0].selected,
    "The first (default) font menuitem is selected again."
  );

  await fontFamilyPrefChanged();
  fontFamily = Services.prefs.getCharPref(fontFamilyPref);
  is(fontFamilyField.value, fontFamily, "The font family has been updated.");

  let defaultFontSize = Services.prefs.getIntPref(
    "font.size.variable." + langGroup
  );
  let fontSizeField = doc.getElementById("defaultFontSize");
  is(
    fontSizeField.value,
    "" + defaultFontSize,
    "Font size should be set correctly."
  );

  let promiseSubDialogLoaded = promiseLoadSubDialog(
    "chrome://browser/content/preferences/dialogs/fonts.xhtml"
  );
  doc.getElementById("advancedFonts").click();
  let win = await promiseSubDialogLoaded;
  doc = win.document;

  // Simulate a dumb font backend.
  win.FontBuilder._enumerator = {
    _list: ["MockedFont1", "MockedFont2", "MockedFont3"],
    _defaultFont: null,
    EnumerateFontsAsync(lang, type) {
      return Promise.resolve(this._list);
    },
    EnumerateAllFontsAsync() {
      return Promise.resolve(this._list);
    },
    getDefaultFont() {
      return this._defaultFont;
    },
    getStandardFamilyName(name) {
      return name;
    },
  };
  win.FontBuilder._allFonts = null;
  win.FontBuilder._langGroupSupported = false;

  let langGroupElement = win.Preferences.get("font.language.group");
  let selectLangsField = doc.getElementById("selectLangs");
  let serifField = doc.getElementById("serif");
  let armenian = "x-armn";
  let western = "x-western";

  // Await rebuilding of the font lists, which happens asynchronously in
  // gFontsDialog._selectLanguageGroup.  Testing code needs to call this
  // function and await its resolution after changing langGroupElement's value
  // (or doing anything else that triggers a call to _selectLanguageGroup).
  function fontListsRebuilt() {
    return win.gFontsDialog._selectLanguageGroupPromise;
  }

  langGroupElement.value = armenian;
  await fontListsRebuilt();
  selectLangsField.value = armenian;
  is(serifField.value, "", "Font family should not be set.");

  let armenianSerifElement = win.Preferences.get("font.name.serif.x-armn");

  langGroupElement.value = western;
  await fontListsRebuilt();
  selectLangsField.value = western;

  // Simulate a font backend supporting language-specific enumeration.
  // NB: FontBuilder has cached the return value from EnumerateAllFonts(),
  // so _allFonts will always have 3 elements regardless of subsequent
  // _list changes.
  win.FontBuilder._enumerator._list = ["MockedFont2"];

  langGroupElement.value = armenian;
  await fontListsRebuilt();
  selectLangsField.value = armenian;
  is(
    serifField.value,
    "",
    "Font family should still be empty for indicating using 'default' font."
  );

  langGroupElement.value = western;
  await fontListsRebuilt();
  selectLangsField.value = western;

  // Simulate a system that has no fonts for the specified language.
  win.FontBuilder._enumerator._list = [];

  langGroupElement.value = armenian;
  await fontListsRebuilt();
  selectLangsField.value = armenian;
  is(serifField.value, "", "Font family should not be set.");

  // Setting default font to "MockedFont3".  Then, when serifField.value is
  // empty, it should indicate using "MockedFont3" but it shouldn't be saved
  // to "MockedFont3" in the pref.  It should be resolved at runtime.
  win.FontBuilder._enumerator._list = [
    "MockedFont1",
    "MockedFont2",
    "MockedFont3",
  ];
  win.FontBuilder._enumerator._defaultFont = "MockedFont3";
  langGroupElement.value = armenian;
  await fontListsRebuilt();
  selectLangsField.value = armenian;
  is(
    serifField.value,
    "",
    "Font family should be empty even if there is a default font."
  );

  armenianSerifElement.value = "MockedFont2";
  serifField.value = "MockedFont2";
  is(
    serifField.value,
    "MockedFont2",
    'Font family should be "MockedFont2" for now.'
  );

  langGroupElement.value = western;
  await fontListsRebuilt();
  selectLangsField.value = western;
  is(serifField.value, "", "Font family of other language should not be set.");

  langGroupElement.value = armenian;
  await fontListsRebuilt();
  selectLangsField.value = armenian;
  is(
    serifField.value,
    "MockedFont2",
    "Font family should not be changed even after switching the language."
  );

  // If MochedFont2 is removed from the system, the value should be treated
  // as empty (i.e., 'default' font) after rebuilding the font list.
  win.FontBuilder._enumerator._list = ["MockedFont1", "MockedFont3"];
  win.FontBuilder._enumerator._allFonts = ["MockedFont1", "MockedFont3"];
  serifField.removeAllItems(); // This will cause rebuilding the font list from available fonts.
  langGroupElement.value = armenian;
  await fontListsRebuilt();
  selectLangsField.value = armenian;
  is(
    serifField.value,
    "",
    "Font family should become empty due to the font uninstalled."
  );

  gBrowser.removeCurrentTab();
});
