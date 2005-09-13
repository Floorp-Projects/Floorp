# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
# The Original Code is the Thunderbird Preferences System.
#
# The Initial Developer of the Original Code is
# Scott MacGregor.
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

var gDisplayPane = {
  mInitialized: false,

  init: function ()
  {
    var preference = document.getElementById("mail.preferences.display.selectedTabIndex");
    if (preference.value)
      document.getElementById("displayPrefs").selectedIndex = preference.value;
    this.mInitialized = true;
  },

  tabSelectionChanged: function ()
  {
    if (this.mInitialized)
      document.getElementById("mail.preferences.display.selectedTabIndex")
              .valueFromPreferences = document.getElementById("displayPrefs").selectedIndex;
  },

  restoreDefaultLabels: function()
  {
    for (var index = 1; index <= 5; index++)
    {
      // reset throws an exception if the pref value is already the default so
      // work around that with some try/catch exception handling
      try {
        document.getElementById('mailnews.labels.description.' + index ).reset();
      } catch (ex) {}

      try {
        document.getElementById('mailnews.labels.color.' + index ).reset();
      } catch (ex) {}
    }
  },

  fontOptionsDialog: function()
  {
    document.documentElement.openSubDialog("chrome://messenger/content/preferences/fonts.xul", "", null);  
  },
  
  mCharsetMenuInitialized: false,
  readDefaultCharset: function()
  {
    if (!this.mCharsetMenuInitialized) 
    {
      Components.classes["@mozilla.org/observer-service;1"]
                .getService(Components.interfaces.nsIObserverService)
                .notifyObservers(null, "charsetmenu-selected", "mailedit");
      this.mCharsetMenuInitialized = true;
    }
    return undefined;
  }
};
