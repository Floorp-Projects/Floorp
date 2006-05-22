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

var gGeneralPane = {
  mPane: null,
  mSound: null,

  init: function ()
  {
    this.mPane = document.getElementById("paneGeneral"); 
    
    this.startPageCheck();
    this.mailSoundCheck();
    
#ifdef XP_WIN
    document.getElementById('mail.checkDefaultMail').valueFromPreferences = this.onReadDefaultMailPref();
    document.getElementById('mail.checkDefaultNews').valueFromPreferences = this.onReadDefaultNewsPref();
    document.getElementById('mail.checkDefaultFeed').valueFromPreferences = this.onReadDefaultFeedPref();
#endif

#ifdef HAVE_SHELL_SERVICE
  // first check whether nsIMapiRegistry is available.  if it's not,
  // hide the whole default mail/news app section.
  var mapiReg;
  try {
    mapiReg = Components.classes["@mozilla.org/mapiregistry;1"].getService(Components.interfaces.nsIMapiRegistry);
  } catch (e) {}
  
  if (!mapiReg) 
    document.getElementById("defaultClientBox").hidden = true;
#endif
  },

#ifdef XP_WIN
  onReadDefaultMailPref: function()
  {
    var mapiRegistry = Components.classes["@mozilla.org/mapiregistry;1"].getService(Components.interfaces.nsIMapiRegistry);
    document.getElementById('defaultMailClient').checked = mapiRegistry.isDefaultMailClient;
    return mapiRegistry.isDefaultMailClient;
  },

  onWriteDefaultMailPref: function()
  {
    var mapiRegistry = Components.classes["@mozilla.org/mapiregistry;1"].getService(Components.interfaces.nsIMapiRegistry);
    var makeDefaultMailClient = document.getElementById('mail.checkDefaultMail').value;   
    if (mapiRegistry.isDefaultMailClient != makeDefaultMailClient) 
      mapiRegistry.isDefaultMailClient = makeDefaultMailClient;
  },

  onReadDefaultNewsPref: function()
  {
    var mapiRegistry = Components.classes["@mozilla.org/mapiregistry;1"].getService(Components.interfaces.nsIMapiRegistry);
    document.getElementById('defaultNewsClient').checked = mapiRegistry.isDefaultNewsClient;   
    return mapiRegistry.isDefaultNewsClient;   
  },

  onWriteDefaultNewsPref: function()
  {
    var mapiRegistry = Components.classes["@mozilla.org/mapiregistry;1"].getService(Components.interfaces.nsIMapiRegistry);
    var makeDefaultNewsClient = document.getElementById('mail.checkDefaultNews').value;   
    if (mapiRegistry.isDefaultNewsClient != makeDefaultNewsClient) 
      mapiRegistry.isDefaultNewsClient = makeDefaultNewsClient;
  },

  onReadDefaultFeedPref: function()
  {
    var mapiRegistry = Components.classes["@mozilla.org/mapiregistry;1"].getService(Components.interfaces.nsIMapiRegistry);
    document.getElementById('defaultFeedClient').checked = mapiRegistry.isDefaultFeedClient; 
    return mapiRegistry.isDefaultFeedClient;   
  },

  onWriteDefaultFeed: function()
  {
    var mapiRegistry = Components.classes["@mozilla.org/mapiregistry;1"].getService(Components.interfaces.nsIMapiRegistry);
    var makeDefaultFeedClient = document.getElementById('mail.checkDefaultFeed').value;  
    if (mapiRegistry.isDefaultFeedClient != makeDefaultFeedClient) 
      mapiRegistry.isDefaultFeedClient = makeDefaultFeedClient;
  },

#endif
  startPageCheck: function() 
  {
    document.getElementById("mailnewsStartPageUrl").disabled = !document.getElementById("mailnewsStartPageEnabled").checked;
  },
  
  setHomePageToDefaultPage: function ()
  {
    var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                                .getService(Components.interfaces.nsIPrefService);
    var pref = prefService.getDefaultBranch(null);
    var url = pref.getComplexValue("mailnews.start_page.url",
                                   Components.interfaces.nsIPrefLocalizedString).data;
    var startPageUrlField = document.getElementById("mailnewsStartPageUrl");
    startPageUrlField.value = url;
    
    this.mPane.userChangedValue(startPageUrlField);
  },

#ifdef HAVE_SHELL_SERVICE
  openDefaultClientDialog: function () 
  {
    document.documentElement
            .openSubDialog("chrome://messenger/content/preferences/defaultClient.xul",
                           "", null);
  },
#endif

  showConnections: function ()
  {
    document.documentElement
            .openSubDialog("chrome://messenger/content/preferences/connection.xul",
                           "", null);
  },

  mailSoundCheck: function()
  {
    document.getElementById("soundButton").disabled = !document.getElementById("newMailNotification").checked;
  },

  showAdvancedSound: function()
  {
    document.documentElement
            .openSubDialog("chrome://messenger/content/preferences/notifications.xul",
                            "", null);
  }
};
