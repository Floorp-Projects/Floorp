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

const kLDAPPrefContractID="@mozilla.org/ldapprefs-service;1";

var gRefresh = false; // leftover hack from the old preferences dialog

var gComposePane = {
  mInitialized: false,
  mDirectories: null,
  mLDAPPrefsService: null,
  mSpellChecker: null,

  init: function ()
  {
    if (kLDAPPrefContractID in Components.classes)
      this.mLDAPPrefsService = Components.classes[kLDAPPrefContractID].getService(Components.interfaces.nsILDAPPrefsService);

    this.createDirectoriesList();
    
    // build the local address book menu list. We do this by hand instead of using the xul template
    // builder because of Bug #285076, 
    this.createLocalDirectoriesList();
    
    this.enableAutocomplete();

    this.initLanguageMenu();

    document.getElementById('downloadDictionaries').setAttribute('href', xlateURL('urn:clienturl:composer:spellcheckers'));

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

  createLocalDirectoriesList: function () 
  {
    var abPopup = document.getElementById("abPopup-menupopup");

    if (abPopup) 
      this.loadLocalDirectories(abPopup);
  },

  loadLocalDirectories: function (aPopup)
  {
    var rdfService = Components.classes["@mozilla.org/rdf/rdf-service;1"]
                     .getService(Components.interfaces.nsIRDFService);

    var parentDir = rdfService.GetResource("moz-abdirectory://").QueryInterface(Components.interfaces.nsIAbDirectory);
    var enumerator = parentDir.childNodes;
    var preference = document.getElementById("mail.collect_addressbook");

    if (enumerator)
    {
      while (enumerator.hasMoreElements())
      {
        var addrbook = enumerator.getNext();
        if (addrbook instanceof Components.interfaces.nsIAbDirectory && !addrbook.isRemote && !addrbook.isMailList)
        {
          var abURI = addrbook.directoryProperties.URI;
          item = document.createElement("menuitem");
          item.setAttribute("label", addrbook.dirName);
          item.setAttribute("value", abURI);
          aPopup.appendChild(item);   
          if (preference.value == abURI)
          {
            aPopup.parentNode.value = abURI;
            aPopup.selectedItem = item;
          }
        }
      }
    }
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
  },

  initLanguageMenu: function ()
  {
    this.mSpellChecker = Components.classes['@mozilla.org/spellchecker/myspell;1'].getService(Components.interfaces.mozISpellCheckingEngine);
    var o1 = {};
    var o2 = {};
    var languageMenuList = document.getElementById('LanguageMenulist');

    // Get the list of dictionaries from
    // the spellchecker.

    this.mSpellChecker.getDictionaryList(o1, o2);

    var dictList = o1.value;
    var count    = o2.value;

    // Load the string bundles that will help us map
    // RFC 1766 strings to UI strings.

    // Load the language string bundle.
    var languageBundle = document.getElementById("languageBundle");
    var regionBundle;
    // If we have a language string bundle, load the region string bundle.
    if (languageBundle)
      regionBundle = document.getElementById("regionBundle");
  
    var menuStr2;
    var isoStrArray;
    var defaultItem = null;
    var langId;
    var i;

    for (i = 0; i < dictList.length; i++)
    {
      try {
        langId = dictList[i];
        isoStrArray = dictList[i].split("-");

        dictList[i] = new Array(2); // first subarray element - pretty name
        dictList[i][1] = langId;    // second subarray element - language ID

        if (languageBundle && isoStrArray[0])
          dictList[i][0] = languageBundle.getString(isoStrArray[0].toLowerCase());

        if (regionBundle && dictList[i][0] && isoStrArray.length > 1 && isoStrArray[1])
        {
          menuStr2 = regionBundle.getString(isoStrArray[1].toLowerCase());
          if (menuStr2)
            dictList[i][0] = dictList[i][0] + "/" + menuStr2;
        }

        if (!dictList[i][0])
          dictList[i][0] = dictList[i][1];
      } catch (ex) {
        // GetString throws an exception when
        // a key is not found in the bundle. In that
        // case, just use the original dictList string.

        dictList[i][0] = dictList[i][1];
      }
    }
  
    // note this is not locale-aware collation, just simple ASCII-based sorting
    // we really need to add loacel-aware JS collation, see bug XXXXX
    dictList.sort();

    var curLang  = languageMenuList.value;

    // now select the dictionary we are currently using
    for (i = 0; i < dictList.length; i++)
    {
      var item = languageMenuList.appendItem(dictList[i][0], dictList[i][1]);
      if (curLang && dictList[i][1] == curLang)
        defaultItem = item;
    }

    // Now make sure the correct item in the menu list is selected.
    if (defaultItem)
      languageMenuList.selectedItem = defaultItem;
    else
      languageMenuList.selectedIndex = 0;
      
  },
};
