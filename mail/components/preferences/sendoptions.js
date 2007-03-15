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
# Ben Goodger.
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

var gSendOptionsDialog = {
  mPrefsBundle: null,
  mHTMLListBox: null,
  mPlainTextListBox: null,

  init: function ()
  {
    this.mPrefsBundle = document.getElementById('bundlePreferences'); 
    this.mHTMLListBox = document.getElementById('html_domains');
    this.mPlainTextListBox = document.getElementById('plaintext_domains');

    var htmlDomainPrefString = document.getElementById('mailnews.html_domains').value;
    this.loadDomains(document.getElementById('mailnews.html_domains').value, 
                     this.mHTMLListBox);
    this.loadDomains(document.getElementById('mailnews.plaintext_domains').value, 
                     this.mPlainTextListBox);
  },

  saveDomainPref: function(aHTML)
  {
    var listbox = aHTML ? this.mHTMLListBox : this.mPlainTextListBox;
    var num_domains = 0;
    var pref_string = "";

    for (var item = listbox.firstChild; item != null; item = item.nextSibling) 
    {
      var domainid = item.getAttribute('label');
      if (domainid.length > 1) 
      {
        num_domains++;

        //separate >1 domains by commas
        if (num_domains > 1)
          pref_string = pref_string + "," + domainid;
        else
          pref_string = domainid;
      }
    }
    
    return pref_string;
  },

  loadDomains: function (aPrefString, aListBox)
  {
    var arrayOfPrefs = aPrefString.split(',');
    if (arrayOfPrefs)
      for (var i = 0; i < arrayOfPrefs.length; i++) 
      {
        var str = arrayOfPrefs[i].replace(/ /g,"");
        if (str)
          this.addItemToDomainList(aListBox, str);
      }
  },

  removeDomains: function(aHTML)
  {
    var listbox = aHTML ? this.mHTMLListBox : this.mPlainTextListBox;

    var currentIndex = listbox.currentIndex;

    while (listbox.selectedItems.length > 0) 
      listbox.removeChild(listbox.selectedItems[0]);

    document.getElementById('SendOptionsDialogPane').userChangedValue(listbox);
  },

  addDomain: function (aHTML)
  {
    var listbox = aHTML ? this.mHTMLListBox : this.mPlainTextListBox;
      
    var domainName;
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
    promptService = promptService.QueryInterface(Components.interfaces.nsIPromptService);

    if (promptService)
    {
      var result = {value:null};
      if (promptService.prompt(window, this.mPrefsBundle.getString(listbox.id + 'AddDomainTitle'),
                               this.mPrefsBundle.getString(listbox.id + 'AddDomain'), result, null, {value:0}))
        domainName = result.value.replace(/ /g,"");
    }

    if (domainName && !this.domainAlreadyPresent(domainName))
    {
      this.addItemToDomainList(listbox, domainName);
      document.getElementById('SendOptionsDialogPane').userChangedValue(listbox);
    }

  },

  domainAlreadyPresent: function(aDomainName)
  {
    var matchingDomains = this.mHTMLListBox.getElementsByAttribute('label', aDomainName);
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService();
    promptService = promptService.QueryInterface(Components.interfaces.nsIPromptService);

    if (!matchingDomains.length)
      matchingDomains = this.mPlainTextListBox.getElementsByAttribute('label', aDomainName);

    if (matchingDomains.length)
    {
      promptService.alert(window, this.mPrefsBundle.getString('domainNameErrorTitle'), 
                         this.mPrefsBundle.getFormattedString("domainDuplicationError", [aDomainName]));
    }

    return matchingDomains.length;
  },

  addItemToDomainList: function (aListBox, aDomainTitle)
  {
    var item = document.createElement('listitem');
    item.setAttribute('label', aDomainTitle);
    aListBox.appendChild(item);
  }
};
