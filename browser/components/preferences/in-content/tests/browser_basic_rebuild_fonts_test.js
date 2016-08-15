Services.prefs.setBoolPref("browser.preferences.instantApply", true);

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("browser.preferences.instantApply");
});

add_task(function*() {
  yield openPreferencesViaOpenPreferencesAPI("paneContent", null, {leaveOpen: true});
  let doc = gBrowser.contentDocument;
  var langGroup = Services.prefs.getComplexValue("font.language.group", Ci.nsIPrefLocalizedString).data
  is(doc.getElementById("font.language.group").value, langGroup,
     "Language group should be set correctly.");

  let defaultFontType = Services.prefs.getCharPref("font.default." + langGroup);
  let fontFamily = Services.prefs.getCharPref("font.name." + defaultFontType + "." + langGroup);
  let fontFamilyField = doc.getElementById("defaultFont");
  is(fontFamilyField.value, fontFamily, "Font family should be set correctly.");

  let defaultFontSize = Services.prefs.getIntPref("font.size.variable." + langGroup);
  let fontSizeField = doc.getElementById("defaultFontSize");
  is(fontSizeField.value, defaultFontSize, "Font size should be set correctly.");

  doc.getElementById("advancedFonts").click();
  let win = yield promiseLoadSubDialog("chrome://browser/content/preferences/fonts.xul");
  doc = win.document;

  // Simulate a dumb font backend.
  win.FontBuilder._enumerator = {
    _list: ["MockedFont1", "MockedFont2", "MockedFont3"],
    EnumerateFonts: function(lang, type, list) {
      return this._list;
    },
    EnumerateAllFonts: function() {
      return this._list;
    },
    getDefaultFont: function() { return null; },
    getStandardFamilyName: function(name) { return name; },
  };
  win.FontBuilder._allFonts = null;
  win.FontBuilder._langGroupSupported = false;

  let langGroupElement = doc.getElementById("font.language.group");
  let selectLangsField = doc.getElementById("selectLangs");
  let serifField = doc.getElementById("serif");
  let armenian = "x-armn";
  let western = "x-western";

  langGroupElement.value = armenian;
  selectLangsField.value = armenian;
  is(serifField.value, "", "Font family should not be set.");

  langGroupElement.value = western;
  selectLangsField.value = western;

  // Simulate a font backend supporting language-specific enumeration.
  // NB: FontBuilder has cached the return value from EnumerateAllFonts(),
  // so _allFonts will always have 3 elements regardless of subsequent
  // _list changes.
  win.FontBuilder._enumerator._list = ["MockedFont2"];

  langGroupElement.value = armenian;
  selectLangsField.value = armenian;
  is(serifField.value, "MockedFont2", "Font family should be set.");

  langGroupElement.value = western;
  selectLangsField.value = western;

  // Simulate a system that has no fonts for the specified language.
  win.FontBuilder._enumerator._list = [];

  langGroupElement.value = armenian;
  selectLangsField.value = armenian;
  is(serifField.value, "", "Font family should not be set.");

  gBrowser.removeCurrentTab();
});
