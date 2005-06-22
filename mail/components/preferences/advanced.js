# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
# The Original Code is the Thunderbird Preferences System.
# 
# The Initial Developer of the Original Code is Scott MacGregor.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#   Scott MacGregor <mscott@mozilla.org>
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
  mPane: null,
  mInitialized: false,

  init: function ()
  {
    this.mPane = document.getElementById("paneAdvanced");
    this.updateMarkAsReadTextbox(false);

    if ("arguments" in window && window.arguments[1] && document.getElementById(window.arguments[1]))
      document.getElementById("advancedPrefs").selectedTab = document.getElementById(window.arguments[1]);
    else 
    {    
      var preference = document.getElementById("mail.preferences.advanced.selectedTabIndex");
      if (preference.value)
        document.getElementById("advancedPrefs").selectedIndex = preference.value;
    }
    this.mInitialized = true;
  },

  tabSelectionChanged: function ()
  {
    if (this.mInitialized)
    {
      document.getElementById("mail.preferences.advanced.selectedTabIndex")
              .valueFromPreferences = document.getElementById("advancedPrefs").selectedIndex;
    }
  },

  showConfigEdit: function()
  {
    document.documentElement.openWindow("Preferences:ConfigManager",
                                        "chrome://global/content/config.xul",
                                        "", null);
  },

  updateAppUpdateUI: function ()
  {
    var preference = document.getElementById("app.update.autoUpdateEnabled");
    var ids = ["enableAutoInstall", "autoInstallMode", "updateAnd"];
    if (!preference.value)
      for (var i = 0; i < ids.length; ++i)
        document.getElementById(ids[i]).disabled = true;
    else 
    {
      document.getElementById("enableAutoInstall").disabled = false;
      this.updateAutoInstallUI();
    }
    return undefined;
  },
  
  updateAutoInstallUI: function ()
  {
    var preference = document.getElementById("app.update.autoInstallEnabled");
    var ids = ["autoInstallMode", "updateAnd"];
    for (var i = 0; i < ids.length; ++i)
      document.getElementById(ids[i]).disabled = !preference.value;
    return undefined;
  },
  
  checkForUpdates: function (aType)
  {
    var updates = Components.classes["@mozilla.org/updates/update-service;1"]
                            .getService(Components.interfaces.nsIUpdateService);
    updates.checkForUpdates([], 0, aType, 
                            Components.interfaces.nsIUpdateService.SOURCE_EVENT_USER,
                            null);  
  },

  updateMarkAsReadTextbox: function(aFocusTextBox) 
  {
    var textbox = document.getElementById('markAsReadDelay'); 
    textbox.disabled = !document.getElementById('markAsRead').checked;
    if (!textbox.disabled && aFocusTextBox)
        textbox.focus();
  }
};
