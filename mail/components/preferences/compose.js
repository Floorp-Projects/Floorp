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

const kLDAPPrefContractID="@mozilla.org/ldapprefs-service;1";

var gRefresh = false; // leftover hack from the old preferences dialog

var gComposePane = {
  mInitialized: false,
  mDirectories: null,
  mLDAPPrefsService: null,

  init: function ()
  {
    if (kLDAPPrefContractID in Components.classes)
      this.mLDAPPrefsService = Components.classes[kLDAPPrefContractID].getService(Components.interfaces.nsILDAPPrefsService);

    this.createDirectoriesList();
    this.enableAutocomplete();

    var preference = document.getElementById("mail.preferences.compose.selectedTabIndex");
    if (preference.value)
      document.getElementById("composePrefs").selectedIndex = preference.value;
    this.mInitialized = true;
  },

  tabSelectionChanged: function ()
  {
    if (this.mInitialized)
    {
      var preference = document.getElementById("mail.preferences.compose.selectedTabIndex");
      preference.valueFromPreferences = document.getElementById("composePrefs").selectedIndex;
    }
  },

  showReturnReceipts: function()
  {
    document.documentElement.openSubDialog("chrome://messenger/content/preferences/receipts.xul",
                                           "", null);
  },

  sendOptionsDialog: function()
  {
    document.documentElement.openSubDialog("chrome://messenger/content/preferences/sendoptions.xul","", null);    
  },

  htmlComposeDialog: function()
  {
    document.documentElement.openSubDialog("chrome://messenger/content/preferences/htmlcompose.xul","", null);  
  },

  enableAutocomplete: function() 
  {
    var directoriesList =  document.getElementById("directoriesList"); 
    var directoriesListPopup = document.getElementById("directoriesListPopup");
    var editButton = document.getElementById("editButton");

    if (document.getElementById("autocompleteLDAP").checked) 
    {
      editButton.removeAttribute("disabled");
      directoriesList.removeAttribute("disabled");
      directoriesListPopup.removeAttribute("disabled");
    }
    else 
    {
      directoriesList.setAttribute("disabled", true);
      directoriesListPopup.setAttribute("disabled", true);
      editButton.setAttribute("disabled", true);
    }

    // if we do not have any directories disable the dropdown list box
    if (!this.mDirectories || (this.mDirectories < 1))
      directoriesList.setAttribute("disabled", true);  
  },

  createDirectoriesList: function()
  {
    var directoriesListPopup = document.getElementById("directoriesListPopup");

    if (directoriesListPopup) 
      this.loadDirectories(directoriesListPopup);
  },

  loadDirectories: function(aPopup)
  {
    var prefCount = {value:0};
    var description = "";
    var item;
    var j=0;
    var arrayOfDirectories;
    var position;
    var dirType;
    var directoriesList;
    var prefService;
    
    prefService = Components.classes["@mozilla.org/preferences-service;1"]
                            .getService(Components.interfaces.nsIPrefBranch);
    
    if (!this.mDirectories) 
    {
      try 
      {
        if (this.mLDAPPrefsService)
          arrayOfDirectories = this.mLDAPPrefsService.getServerList(prefService, prefCount);
      }
      catch (ex) {}

      if (arrayOfDirectories) 
      {
        this.mDirectories = new Array();
        for (var i = 0; i < prefCount.value; i++)
        {
          if ((arrayOfDirectories[i] != "ldap_2.servers.pab") && 
            (arrayOfDirectories[i] != "ldap_2.servers.history")) 
          {
            try 
            {
              position = prefService.getIntPref(arrayOfDirectories[i]+".position");
            }
             catch(ex)
            {
              position = 1;
            }
          
            try
            {
              dirType = prefService.getIntPref(arrayOfDirectories[i]+".dirType");
            }
            catch(ex)
            {
              dirType = 1;
            }

            if ((position != 0) && (dirType == 1)) 
            {
              try
              {
                description = prefService.getComplexValue(arrayOfDirectories[i]+".description",
                                                       Components.interfaces.nsISupportsString).data;
              }
              catch(ex)
              {
                description="";
              }
              
              if (description != "") 
              {
                if (aPopup) 
                {
                  item = document.createElement("menuitem");
                  item.setAttribute("label", description);
                  item.setAttribute("value", arrayOfDirectories[i]);
                  aPopup.appendChild(item);
                }
              
                this.mDirectories[j++] = {value:arrayOfDirectories[i], label:description};
              }
            }
          }
        }
      
        var value;
        if (aPopup) 
        {
          directoriesList =  document.getElementById("directoriesList");
          if (gRefresh) 
          {
            // gRefresh is true if user edits, removes or adds a directory.
            value = directoriesList.value;
            directoriesList.selectedItem = null;
            directoriesList.value = value;
            if (!directoriesList.selectedItem)
              directoriesList.selectedIndex = 0;
            if (!directoriesList.selectedItem) 
            {
              directoriesList.value = "";
              directoriesList.disabled = true;
            }
            else if (!prefService.prefIsLocked("ldap_2.autoComplete.directoryServer"))
              directoriesList.disabled = false;
            return;
          }
        
          var pref_string_title = "ldap_2.autoComplete.directoryServer";
          try 
          {
            var directoryServer = prefService.getCharPref(pref_string_title);
            directoriesList.value = directoryServer;
            if (!directoriesList.selectedItem)
              directoriesList.selectedIndex = 0;
            if (!directoriesList.selectedItem)
              directoriesList.value = "";
          }
          catch (ex)
          {
            directoriesList.selectedItem = null;
          }
        }
      }
    }
  },

  editDirectories: function()
  {
    var args = {fromGlobalPref: true};
    window.openDialog("chrome://messenger/content/addressbook/pref-editdirectories.xul",
                      "editDirectories", "chrome,modal=yes,resizable=no", args);
    if (gRefresh)
    {
      var popup = document.getElementById("directoriesListPopup");
      if (popup) 
        while (popup.hasChildNodes())
          popup.removeChild(popup.lastChild);

    }

    this.mDirectories = null;
    this.loadDirectories(popup);
    gRefresh = false;
  }

};
