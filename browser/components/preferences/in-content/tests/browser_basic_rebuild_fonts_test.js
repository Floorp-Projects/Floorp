Services.prefs.setBoolPref("browser.preferences.instantApply", true);

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("browser.preferences.instantApply");
});

add_task(function() {
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

  gBrowser.removeCurrentTab();
});
