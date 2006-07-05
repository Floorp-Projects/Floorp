# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Firefox Preferences System.
#
# The Initial Developer of the Original Code is
# Ben Goodger.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben Goodger <ben@mozilla.org>
#   Asaf Romano <mozilla.mano@sent.com>
#   Jeff Walden <jwalden+code@mit.edu>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# browser.display.languageList LOCK ALL when LOCKED

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
  init: function ()
  {
    this._rebuildFonts();
    var menulist = document.getElementById("defaultFont");
    if (menulist.selectedIndex == -1) {
      menulist.insertItemAt(0, "", "", "");
      menulist.selectedIndex = 0;
    }
  },

  _rebuildFonts: function ()
  {
    var langGroupPref = document.getElementById("font.language.group");
    this._selectDefaultLanguageGroup(langGroupPref.value,
                                     this._readDefaultFontTypeForLanguage(langGroupPref.value) == "serif");
  },

  _selectDefaultLanguageGroup: function (aLanguageGroup, aIsSerif)
  {
    var prefs = [{ format   : aIsSerif ? kFontNameFmtSerif : kFontNameFmtSansSerif,
                   type     : "unichar",
                   element  : "defaultFont",
                   fonttype : aIsSerif ? "serif" : "sans-serif" },
                 { format   : aIsSerif ? kFontNameListFmtSerif : kFontNameListFmtSansSerif,
                   type     : "unichar",
                   element  : null,
                   fonttype : aIsSerif ? "serif" : "sans-serif" },
                 { format   : kFontSizeFmtVariable,
                   type     : "int",
                   element  : "defaultFontSize",
                   fonttype : null }];
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

  _readDefaultFontTypeForLanguage: function (aLanguageGroup)
  {
    var defaultFontTypePref = kDefaultFontType.replace(/%LANG%/, aLanguageGroup);
    var preference = document.getElementById(defaultFontTypePref);
    if (!preference) {
      preference = document.createElement("preference");
      preference.id = defaultFontTypePref;
      preference.setAttribute("name", defaultFontTypePref);
      preference.setAttribute("type", "string");
      preference.setAttribute("onchange", "gContentPane._rebuildFonts();");
      document.getElementById("fontPreferences").appendChild(preference);
    }
    return preference.value;
  },

  _selectLanguageGroup: function (aLanguageGroup)
  {
    var prefs = [{ format: kDefaultFontType,          type: "string", element: "defaultFontType", fonttype: null},
                 { format: kFontNameFmtSerif,         type: "unichar", element: "serif",      fonttype: "serif"       },
                 { format: kFontNameFmtSansSerif,     type: "unichar", element: "sans-serif", fonttype: "sans-serif"  },
                 { format: kFontNameFmtMonospace,     type: "unichar", element: "monospace",  fonttype: "monospace"   },
                 { format: kFontNameListFmtSerif,     type: "unichar", element: null,         fonttype: "serif"       },
                 { format: kFontNameListFmtSansSerif, type: "unichar", element: null,         fonttype: "sans-serif"  },
                 { format: kFontNameListFmtMonospace, type: "unichar", element: null,         fonttype: "monospace"   },
                 { format: kFontSizeFmtVariable,      type: "int",     element: "sizeVar",    fonttype: null          },
                 { format: kFontSizeFmtFixed,         type: "int",     element: "sizeMono",   fonttype: null          },
                 { format: kFontMinSizeFmt,           type: "int",     element: "minSize",    fonttype: null          }];
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
  
  readFontLanguageGroup: function ()
  {
    var languagePref = document.getElementById("font.language.group");
    this._selectLanguageGroup(languagePref.value);
    return undefined;
  },
  
  readFontSelection: function (aElement)
  {
    // Determine the appropriate value to select, for the following cases:
    // - there is no setting 
    // - the font selected by the user is no longer present (e.g. deleted from
    //   fonts folder)
    var preference = document.getElementById(aElement.getAttribute("preference"));
    if (preference.value) {
      var fontItems = aElement.getElementsByAttribute("value", preference.value);
    
      // There is a setting that actually is in the list. Respect it.
      if (fontItems.length > 0)
        return undefined;
    }
    
    var defaultValue = aElement.firstChild.firstChild.getAttribute("value");
    var languagePref = document.getElementById("font.language.group");
    preference = document.getElementById("font.name-list." + aElement.id + "." + languagePref.value);
    if (!preference || !preference.hasUserValue)
      return defaultValue;
    
    var fontNames = preference.value.split(",");
    var stripWhitespace = /^\s*(.*)\s*$/;
    
    for (var i = 0; i < fontNames.length; ++i) {
      var fontName = fontNames[i].replace(stripWhitespace, "$1");
      fontItems = aElement.getElementsByAttribute("value", fontName);
      if (fontItems.length)
        break;
    }
    if (fontItems.length)
      return fontItems[0].getAttribute("value");
    return defaultValue;
  },
  
  _charsetMenuInitialized: false,
  readDefaultCharset: function ()
  {
    if (!this._charsetMenuInitialized) {
      var os = Components.classes["@mozilla.org/observer-service;1"]
                         .getService(Components.interfaces.nsIObserverService);
      os.notifyObservers(null, "charsetmenu-selected", "other");
      this._charsetMenuInitialized = true;
    }
    return undefined;
  },
  
  readUseDocumentFonts: function ()
  {
    var preference = document.getElementById("browser.display.use_document_fonts");
    return preference.value == 1;
  },
  
  writeUseDocumentFonts: function ()
  {
    var useDocumentFonts = document.getElementById("useDocumentFonts");
    return useDocumentFonts.checked ? 1 : 0;
  }
};

