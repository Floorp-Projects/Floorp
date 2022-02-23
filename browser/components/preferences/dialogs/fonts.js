/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../../base/content/utilityOverlay.js */
/* import-globals-from ../../../../toolkit/mozapps/preferences/fontbuilder.js */

// browser.display.languageList LOCK ALL when LOCKED

const kDefaultFontType = "font.default.%LANG%";
const kFontNameFmtSerif = "font.name.serif.%LANG%";
const kFontNameFmtSansSerif = "font.name.sans-serif.%LANG%";
const kFontNameFmtMonospace = "font.name.monospace.%LANG%";
const kFontNameListFmtSerif = "font.name-list.serif.%LANG%";
const kFontNameListFmtSansSerif = "font.name-list.sans-serif.%LANG%";
const kFontNameListFmtMonospace = "font.name-list.monospace.%LANG%";
const kFontSizeFmtVariable = "font.size.variable.%LANG%";
const kFontSizeFmtFixed = "font.size.monospace.%LANG%";
const kFontMinSizeFmt = "font.minimum-size.%LANG%";

document
  .getElementById("FontsDialog")
  .addEventListener("dialoghelp", window.top.openPrefsHelp);
window.addEventListener("load", () => gFontsDialog.onLoad());

Preferences.addAll([
  { id: "font.language.group", type: "wstring" },
  { id: "browser.display.use_document_fonts", type: "int" },
]);

var gFontsDialog = {
  _selectLanguageGroupPromise: Promise.resolve(),

  onLoad() {
    Preferences.addSyncFromPrefListener(
      document.getElementById("selectLangs"),
      () => this.readFontLanguageGroup()
    );
    Preferences.addSyncFromPrefListener(
      document.getElementById("useDocumentFonts"),
      () => this.readUseDocumentFonts()
    );
    Preferences.addSyncToPrefListener(
      document.getElementById("useDocumentFonts"),
      () => this.writeUseDocumentFonts()
    );
    for (let id of ["serif", "sans-serif", "monospace"]) {
      let el = document.getElementById(id);
      Preferences.addSyncFromPrefListener(el, () =>
        FontBuilder.readFontSelection(el)
      );
    }
  },

  _selectLanguageGroup(aLanguageGroup) {
    this._selectLanguageGroupPromise = (async () => {
      // Avoid overlapping language group selections by awaiting the resolution
      // of the previous one.  We do this because this function is re-entrant,
      // as inserting <preference> elements into the DOM sometimes triggers a call
      // back into this function.  And since this function is also asynchronous,
      // that call can enter this function before the previous run has completed,
      // which would corrupt the font menulists.  Awaiting the previous call's
      // resolution avoids that fate.
      await this._selectLanguageGroupPromise;

      var prefs = [
        {
          format: kDefaultFontType,
          type: "string",
          element: "defaultFontType",
          fonttype: null,
        },
        {
          format: kFontNameFmtSerif,
          type: "fontname",
          element: "serif",
          fonttype: "serif",
        },
        {
          format: kFontNameFmtSansSerif,
          type: "fontname",
          element: "sans-serif",
          fonttype: "sans-serif",
        },
        {
          format: kFontNameFmtMonospace,
          type: "fontname",
          element: "monospace",
          fonttype: "monospace",
        },
        {
          format: kFontNameListFmtSerif,
          type: "unichar",
          element: null,
          fonttype: "serif",
        },
        {
          format: kFontNameListFmtSansSerif,
          type: "unichar",
          element: null,
          fonttype: "sans-serif",
        },
        {
          format: kFontNameListFmtMonospace,
          type: "unichar",
          element: null,
          fonttype: "monospace",
        },
        {
          format: kFontSizeFmtVariable,
          type: "int",
          element: "sizeVar",
          fonttype: null,
        },
        {
          format: kFontSizeFmtFixed,
          type: "int",
          element: "sizeMono",
          fonttype: null,
        },
        {
          format: kFontMinSizeFmt,
          type: "int",
          element: "minSize",
          fonttype: null,
        },
      ];
      for (var i = 0; i < prefs.length; ++i) {
        var name = prefs[i].format.replace(/%LANG%/, aLanguageGroup);
        var preference = Preferences.get(name);
        if (!preference) {
          preference = Preferences.add({ id: name, type: prefs[i].type });
        }

        if (!prefs[i].element) {
          continue;
        }

        var element = document.getElementById(prefs[i].element);
        if (element) {
          element.setAttribute("preference", preference.id);

          if (prefs[i].fonttype) {
            await FontBuilder.buildFontList(
              aLanguageGroup,
              prefs[i].fonttype,
              element
            );
          }

          preference.setElementValue(element);
        }
      }
    })().catch(Cu.reportError);
  },

  readFontLanguageGroup() {
    var languagePref = Preferences.get("font.language.group");
    this._selectLanguageGroup(languagePref.value);
    return undefined;
  },

  readUseDocumentFonts() {
    var preference = Preferences.get("browser.display.use_document_fonts");
    return preference.value == 1;
  },

  writeUseDocumentFonts() {
    var useDocumentFonts = document.getElementById("useDocumentFonts");
    return useDocumentFonts.checked ? 1 : 0;
  },
};
