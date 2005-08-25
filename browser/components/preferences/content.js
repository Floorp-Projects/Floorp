# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
# The Initial Developer of the Original Code is Google Inc.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#   Ben Goodger <ben@mozilla.org>
#   Asaf Romano <mozilla.mano@sent.com>
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


const kDefaultFontType          = "font.default.%LANG%";
const kFontNameFmtSerif         = "font.name.serif.%LANG%";
const kFontNameFmtSansSerif     = "font.name.sans-serif.%LANG%";
const kFontNameListFmtSerif     = "font.name-list.serif.%LANG%";
const kFontNameListFmtSansSerif = "font.name-list.sans-serif.%LANG%";
const kFontSizeFmtVariable      = "font.size.variable.%LANG%";

var gContentPane = {
  _pane: null,
  
  init: function ()
  {
    this._pane = document.getElementById("paneContent");
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
    this._selectLanguageGroup(langGroupPref.value, 
                              this._readDefaultFontTypeForLanguage(langGroupPref.value) == "serif");
  },
  
  _selectLanguageGroup: function (aLanguageGroup, aIsSerif)
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
    var preferences = document.getElementById("contentPreferences");
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
      document.getElementById("contentPreferences").appendChild(preference);
    }  
    return preference.value;
  },

  writeEnableImagesPref: function ()
  { 
    var enableImages = document.getElementById("enableImages");
    if (enableImages.checked) {
      var enableRestricted = document.getElementById("enableRestricted");
      return enableRestricted.checked ? 3 : 1;
    }
    return 2;
  },
  
  readEnableImagesPref: function ()
  {
    var pref = document.getElementById("permissions.default.image");
    var enableRestricted = document.getElementById("enableRestricted");    
    enableRestricted.disabled = pref.value == 2;
    return (pref.value == 1 || pref.value == 3);
  },
  
  readEnableRestrictedPref: function ()
  {
    var pref = document.getElementById("permissions.default.image");
    return (pref.value == 3);
  },
  
  _exceptionsParams: {
    install: { blockVisible: false, sessionVisible: false, allowVisible: true, prefilledHost: "", permissionType: "install" },
    popup:   { blockVisible: false, sessionVisible: false, allowVisible: true, prefilledHost: "", permissionType: "popup"   },
    image:   { blockVisible: true,  sessionVisible: false, allowVisible: true, prefilledHost: "", permissionType: "image"   }
  },
  
  showExceptions: function (aPermissionType)
  {
    var bundlePreferences = document.getElementById("bundlePreferences");
    var params = this._exceptionsParams[aPermissionType];
    params.windowTitle = bundlePreferences.getString(aPermissionType + "permissionstitle");
    params.introText = bundlePreferences.getString(aPermissionType + "permissionstext");
    document.documentElement.openWindow("Browser:Permissions",
                                        "chrome://browser/content/preferences/permissions.xul",
                                        "", params);
  },
  
  updateButtons: function (aButtonID, aPreferenceID)
  {
    var button = document.getElementById(aButtonID);
    var preference = document.getElementById(aPreferenceID);
    button.disabled = preference.value != true;
    return undefined;
  },
  
  showAdvancedJS: function ()
  {
    document.documentElement.openSubDialog("chrome://browser/content/preferences/advanced-scripts.xul",
                                           "", null);  
  },

  showFonts: function ()
  {
    document.documentElement.openSubDialog("chrome://browser/content/preferences/fonts.xul",
                                           "", null);  
  },

  showColors: function ()
  {
    document.documentElement.openSubDialog("chrome://browser/content/preferences/colors.xul",
                                           "", null);  
  }
};
