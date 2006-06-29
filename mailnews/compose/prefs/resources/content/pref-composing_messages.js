var gAutoSaveInterval;
var gLastSelectedLang;
var gDictCount = 0;

function Startup() {
  if ("@mozilla.org/spellchecker;1" in Components.classes)
    InitLanguageMenu();
  else
    document.getElementById("spellingGroup").hidden = true;

  gAutoSaveInterval = document.getElementById("autoSaveInterval");
  EnableTextbox(document.getElementById("autoSave"), gAutoSaveInterval, true);
}

function EnableTextbox(aCheckbox, aTextbox, aStartingUp) {
  aTextbox.disabled = (!aCheckbox.checked ||
                       parent.hPrefWindow.getPrefIsLocked(aTextbox.getAttribute("prefstring")));

  if (!aTextbox.disabled && !aStartingUp)
    aTextbox.focus();
}

function PopulateFonts() {
  var fontsList = document.getElementById("fontSelect");
  try {
    var enumerator = Components.classes["@mozilla.org/gfx/fontenumerator;1"]
                               .getService(Components.interfaces.nsIFontEnumerator);
    var localFontCount = { value: 0 }
    var localFonts = enumerator.EnumerateAllFonts(localFontCount);
    for (var i = 0; i < localFonts.length; ++i) {
      if (localFonts[i] != "") {
        fontsList.appendItem(localFonts[i], localFonts[i]);
      }
    }
  } catch (ex) { }
}

function InitLanguageMenu() {
  var spellChecker = Components.classes["@mozilla.org/spellchecker/myspell;1"]
                               .getService(Components.interfaces.mozISpellCheckingEngine);

  var o1 = {};
  var o2 = {};

  // Get the list of dictionaries from the spellchecker.
  spellChecker.getDictionaryList(o1, o2);

  var dictList = o1.value;
  var count    = o2.value;

  // If dictionary count hasn't changed then no need to update the menu.
  if (gDictCount == count)
    return;

  // Store current dictionary count.
  gDictCount = count;

  // Load the string bundles that will help us map
  // RFC 1766 strings to UI strings.

  // Load the language string bundle.
  var languageBundle = document.getElementById("languageBundle");
  var regionBundle = null;
  // If we have a language string bundle, load the region string bundle.
  if (languageBundle)
    regionBundle = document.getElementById("regionBundle");
  
  var menuStr2;
  var isoStrArray;
  var langId;
  var langLabel;
  var i;

  for (i = 0; i < count; i++) {
    try {
      langId = dictList[i];
      isoStrArray = dictList[i].split("-");

      if (languageBundle && isoStrArray[0])
        langLabel = languageBundle.getString(isoStrArray[0].toLowerCase());

      if (regionBundle && langLabel && isoStrArray.length > 1 && isoStrArray[1]) {
        menuStr2 = regionBundle.getString(isoStrArray[1].toLowerCase());
        if (menuStr2)
          langLabel += "/" + menuStr2;
      }

      if (!langLabel)
        langLabel = langId;
    } catch (ex) {
      // getString throws an exception when a key is not found in the
      // bundle. In that case, just use the original dictList string.
      langLabel = langId;
    }
    dictList[i] = [langLabel, langId];
  }
  
  // sort by locale-aware collation
  dictList.sort(
    function compareFn(a, b) {
      return a[0].localeCompare(b[0]);
    }
  );

  var languageMenuList = document.getElementById("languageMenuList");
  // Remove any languages from the list.
  var languageMenuPopup = languageMenuList.firstChild;
  while (languageMenuPopup.firstChild.localName != "menuseparator")
    languageMenuPopup.removeChild(languageMenuPopup.firstChild);

  var curLang  = languageMenuList.value;
  var defaultItem = null;

  for (i = 0; i < count; i++) {
    var item = languageMenuList.insertItemAt(i, dictList[i][0], dictList[i][1]);
    if (curLang && dictList[i][1] == curLang)
      defaultItem = item;
  }

  // Now make sure the correct item in the menu list is selected.
  if (defaultItem) {
    languageMenuList.selectedItem = defaultItem;
    gLastSelectedLang = defaultItem;
  }
}

function SelectLanguage(aTarget) {
  try {
    if (aTarget.value != "more-cmd")
      gLastSelectedLang = aTarget;
    else {
      window.openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no",
                        xlateURL('urn:clienturl:composer:spellcheckers'));
      if (gLastSelectedLang)
        document.getElementById("languageMenuList").selectedItem = gLastSelectedLang;
    }
  } catch (ex) {
    dump(ex);
  }
}
