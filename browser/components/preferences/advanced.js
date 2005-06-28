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
# The Initial Developer of the Original Code is Ben Goodger.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#   Ben Goodger <ben@mozilla.org>
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

var gAdvancedPane = {
  _inited: false,
  init: function ()
  {
    this._inited = true;
    var advancedPrefs = document.getElementById("advancedPrefs");
    var preference = document.getElementById("browser.preferences.advanced.selectedTabIndex");
    if (preference.value === null)
      return;
    advancedPrefs.selectedIndex = preference.value;
  },
  
  tabSelectionChanged: function ()
  {
    if (!this._inited)
      return;
    var advancedPrefs = document.getElementById("advancedPrefs");
    var preference = document.getElementById("browser.preferences.advanced.selectedTabIndex");
    preference.valueFromPreferences = advancedPrefs.selectedIndex;
  },
  
  updateButtons: function (aButtonID, aPreferenceID)
  {
    var button = document.getElementById(aButtonID);
    var preference = document.getElementById(aPreferenceID);
    // This is actually before the value changes, so the value is not as you expect. 
    button.disabled = preference.value == true;
    return undefined;
  },

  showCertificates: function ()
  {
    document.documentElement.openWindow("mozilla:certmanager",
                                        "chrome://pippki/content/certManager.xul",
                                        "width=600,height=400", null);
  },
  
  showCRLs: function ()
  {
    document.documentElement.openWindow("Mozilla:CRLManager", 
                                        "chrome://pippki/content/crlManager.xul",
                                        "width=600,height=400", null);
  },
  
  showOCSP: function ()
  {
    document.documentElement.openSubDialog("chrome://mozapps/content/preferences/ocsp.xul",
                                           "", null);
  },
  
  showSecurityDevices: function ()
  {
    document.documentElement.openWindow("mozilla:devicemanager",
                                        "chrome://pippki/content/device_manager.xul",
                                        "width=600,height=400", null);
  },
  
  updateAppUpdateUI: function ()
  {
    var preference = document.getElementById("app.update.enabled");
    document.getElementById("enableAutoInstall").disabled = !preference.value;

    var aus = 
        Components.classes["@mozilla.org/updates/update-service;1"].
        getService(Components.interfaces.nsIApplicationUpdateService);
    var checkNowButton = document.getElementById("checkNowButton");
    checkNowButton.disabled = !aus.canUpdate;
    
    this.updateAutoInstallUI();
    return undefined;
  },
  
  updateAutoInstallUI: function ()
  {
    var autoInstallPref = document.getElementById("app.update.autoInstallEnabled");
    var updateEnabledPref = document.getElementById("app.update.enabled");
    var ids = ["autoInstallMode", "updateAnd"];
    var disabled = !updateEnabledPref.value || !autoInstallPref.value;
    for (var i = 0; i < ids.length; ++i)
      document.getElementById(ids[i]).disabled = disabled;
    return undefined;
  },
  
  checkForAddonUpdates: function ()
  {
    var updateTypes = 
        Components.classes["@mozilla.org/supports-PRUint8;1"].
        createInstance(Components.interfaces.nsISupportsPRUint8);
    updateTypes.data = Components.interfaces.nsIUpdateItem.TYPE_ADDON;
    var showMismatch = 
        Components.classes["@mozilla.org/supports-PRBool;1"].
        createInstance(Components.interfaces.nsISupportsPRBool);
    showMismatch.data = false;
    var ary = 
        Components.classes["@mozilla.org/supports-array;1"].
        createInstance(Components.interfaces.nsISupportsArray);
    ary.AppendElement(updateTypes);
    ary.AppendElement(showMismatch);

    var features = "chrome,centerscreen,dialog,titlebar";
    const URI_EXTENSION_UPDATE_DIALOG = 
      "chrome://mozapps/content/extensions/update.xul";
    var ww = 
        Components.classes["@mozilla.org/embedcomp/window-watcher;1"].
        getService(Components.interfaces.nsIWindowWatcher);
    ww.openWindow(window, URI_EXTENSION_UPDATE_DIALOG, "", features, ary);  
  },
  
  checkForUpdates: function ()
  {
    var prompter = Components.classes["@mozilla.org/updates/update-prompt;1"]
                             .createInstance(Components.interfaces.nsIUpdatePrompt);
    prompter.checkForUpdates(window);
  },
  
  showUpdates: function ()
  {
    var prompter = Components.classes["@mozilla.org/updates/update-prompt;1"]
                             .createInstance(Components.interfaces.nsIUpdatePrompt);
    prompter.showUpdateHistory(window);
  },
  
  showLanguages: function ()
  {
    document.documentElement.openSubDialog("chrome://browser/content/preferences/languages.xul",
                                           "", null);  
  }
};

