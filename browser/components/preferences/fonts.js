/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from ../../../toolkit/mozapps/preferences/fontbuilder.js */

// browser.display.languageList LOCK ALL when LOCKED

const kDefaultFontType          = "font.default.%LANG%";
const kFontNameFmtSerif         = "font.name.serif.%LANG%";
const kFontNameFmtSansSerif     = "font.name.sans-serif.%LANG%";
const kFontNameFmtMonospace     = "font.name.monospace.%LANG%";
const kFontNameListFmtSerif     = "font.name-list.serif.%LANG%";
const kFontNameListFmtSansSerif = "font.name-list.sans-serif.%LANG%";
const kFontNameListFmtMonospace = "font.name-list.monospace.%LANG%";
const kFontSizeFmtVariable      = "font.size.variable.%LANG%";
const kFontSizeFmtFixed         = "font.size.fixed.%LANG%";
const kFontMinSizeFmt           = "font.minimum-size.%LANG%";

var gFontsDialog = {
  _selectLanguageGroup(aLanguageGroup)
  {
    var prefs = [{ format: kDefaultFontType,          type: "string",   element: "defaultFontType", fonttype: null},
                 { format: kFontNameFmtSerif,         type: "fontname", element: "serif",      fonttype: "serif"       },
                 { format: kFontNameFmtSansSerif,     type: "fontname", element: "sans-serif", fonttype: "sans-serif"  },
                 { format: kFontNameFmtMonospace,     type: "fontname", element: "monospace",  fonttype: "monospace"   },
                 { format: kFontNameListFmtSerif,     type: "unichar",  element: null,         fonttype: "serif"       },
                 { format: kFontNameListFmtSansSerif, type: "unichar",  element: null,         fonttype: "sans-serif"  },
                 { format: kFontNameListFmtMonospace, type: "unichar",  element: null,         fonttype: "monospace"   },
                 { format: kFontSizeFmtVariable,      type: "int",      element: "sizeVar",    fonttype: null          },
                 { format: kFontSizeFmtFixed,         type: "int",      element: "sizeMono",   fonttype: null          },
                 { format: kFontMinSizeFmt,           type: "int",      element: "minSize",    fonttype: null          }];
    var preferences = document.getElementById("fontPreferences");
    for (var i = 0; i < prefs.length; ++i) {
      var preference = document.getElementById(prefs[i].format.replace(/%LANG%/, aLanguageGroup));
      if (!preference) {
        preference = document.createElement("preference");
        var name = prefs[i].format.replace(/%LANG%/, aLanguageGroup);
        preference.id = name;
        preference.setAttribute("name", name);
        preference.setAttribute("type", prefs[i].type);
        preferences.appendChild(preference);
      }

      if (!prefs[i].element)
        continue;

      var element = document.getElementById(prefs[i].element);
      if (element) {
        element.setAttribute("preference", preference.id);

        if (prefs[i].fonttype)
          FontBuilder.buildFontList(aLanguageGroup, prefs[i].fonttype, element);

        preference.setElementValue(element);
      }
    }
  },

  readFontLanguageGroup()
  {
    var languagePref = document.getElementById("font.language.group");
    this._selectLanguageGroup(languagePref.value);
    return undefined;
  },

  readUseDocumentFonts()
  {
    var preference = document.getElementById("browser.display.use_document_fonts");
    return preference.value == 1;
  },

  writeUseDocumentFonts()
  {
    var useDocumentFonts = document.getElementById("useDocumentFonts");
    return useDocumentFonts.checked ? 1 : 0;
  },

  onBeforeAccept()
  {
    let preferences = document.querySelectorAll("preference[id*='font.minimum-size']");
    // It would be good if we could avoid touching languages the pref pages won't use, but
    // unfortunately the language group APIs (deducing language groups from language codes)
    // are C++ - only. So we just check all the things the user touched:
    // Don't care about anything up to 24px, or if this value is the same as set previously:
    preferences = Array.filter(preferences, prefEl => {
      return prefEl.value > 24 && prefEl.value != prefEl.valueFromPreferences;
    });
    if (!preferences.length) {
      return true;
    }

    let strings = document.getElementById("bundlePreferences");
    let title = strings.getString("veryLargeMinimumFontTitle");
    let confirmLabel = strings.getString("acceptVeryLargeMinimumFont");
    let warningMessage = strings.getString("veryLargeMinimumFontWarning");
    let {Services} = Components.utils.import("resource://gre/modules/Services.jsm", {});
    let flags = Services.prompt.BUTTON_POS_1 * Services.prompt.BUTTON_TITLE_CANCEL |
                Services.prompt.BUTTON_POS_0 * Services.prompt.BUTTON_TITLE_IS_STRING |
                Services.prompt.BUTTON_POS_1_DEFAULT;
    let buttonChosen = Services.prompt.confirmEx(window, title, warningMessage, flags, confirmLabel, null, "", "", {});
    return buttonChosen == 0;
  },
};
