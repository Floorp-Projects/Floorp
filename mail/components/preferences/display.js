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
  mTagListBox:  null,

  init: function ()
  {
    var preference = document.getElementById("mail.preferences.display.selectedTabIndex");
    if (preference.value)
      document.getElementById("displayPrefs").selectedIndex = preference.value;

      // build the charset menu list. We do this by hand instead of using the xul template
      // builder because of Bug #285076, 
      this.createCharsetMenus(document.getElementById("viewDefaultCharset-menupopup"), "NC:DecodersRoot",
                              document.getElementById('mailnews.view_default_charset').value);

    this.mInitialized = true;
    
    this.mTagListBox = document.getElementById('tagList');    
    this.buildTagList();
  },

  tabSelectionChanged: function ()
  {
    if (this.mInitialized)
      document.getElementById("mail.preferences.display.selectedTabIndex")
              .valueFromPreferences = document.getElementById("displayPrefs").selectedIndex;
  },

  fontOptionsDialog: function()
  {
    document.documentElement.openSubDialog("chrome://messenger/content/preferences/fonts.xul", "", null);  
  },
  
  // appends the tag to the tag list box
  appendTagItem: function(aTagName, aKey, aColor)
  {
    var item = this.mTagListBox.appendItem(aTagName, aKey);
    item.style.color = aColor;
    return item;
  },
   
  buildTagList: function()
  {
    var tagService = Components.classes["@mozilla.org/messenger/tagservice;1"].getService(Components.interfaces.nsIMsgTagService);
    var allTags = tagService.tagEnumerator;
    var allKeys = tagService.keyEnumerator;
    while (allTags.hasMore())
    {
      var key = allKeys.getNext();
      this.appendTagItem(allTags.getNext(), key, tagService.getColorForKey(key));
    }
  },
  
  removeTag: function()
  {
    var tagItemToRemove = this.mTagListBox.getSelectedItem();
    var index = this.mTagListBox.selectedIndex;
    if (index >= 0)
    {
      var itemToRemove = this.mTagListBox.getItemAtIndex(index);
      var tagService = Components.classes["@mozilla.org/messenger/tagservice;1"].getService(Components.interfaces.nsIMsgTagService);
      tagService.deleteKey(itemToRemove.getAttribute("value"));
      this.mTagListBox.removeItemAt(index);
      var numItemsInListBox = this.mTagListBox.getRowCount();
      this.mTagListBox.selectedIndex = index < numItemsInListBox ? index : numItemsInListBox - 1;
    }
  },

  addTag: function()
  {  
    var args = {result: "", okCallback: addTagCallback};
    var dialog = window.openDialog(
			    "chrome://messenger/content/newTagDialog.xul",
			    "",
			    "chrome,titlebar,modal",
			    args);
  },
     
  addMenuItem: function(aMenuPopup, aLabel, aValue)
  { 
    var menuItem = document.createElement('menuitem');
    menuItem.setAttribute('label', aLabel);
    menuItem.setAttribute('value', aValue);
    aMenuPopup.appendChild(menuItem);
  },

  readRDFString: function(aDS,aRes,aProp) 
  {
    var n = aDS.GetTarget(aRes, aProp, true);
    return (n) ? n.QueryInterface(Components.interfaces.nsIRDFLiteral).Value : "";
  },

  createCharsetMenus: function(aMenuPopup, aRoot, aPreferenceValue)
  {
    var rdfService = Components.classes["@mozilla.org/rdf/rdf-service;1"]
                     .getService(Components.interfaces.nsIRDFService);
    var kNC_Root = rdfService.GetResource(aRoot);
    var kNC_Name = rdfService.GetResource("http://home.netscape.com/NC-rdf#Name");

    var rdfDataSource = rdfService.GetDataSource("rdf:charset-menu");
    var rdfContainer = Components.classes["@mozilla.org/rdf/container;1"].getService(Components.interfaces.nsIRDFContainer);
    rdfContainer.Init(rdfDataSource, kNC_Root);

    var charset;
    var availableCharsets = rdfContainer.GetElements();

    for (var i = 0; i < rdfContainer.GetCount(); i++) 
    {
      charset = availableCharsets.getNext().QueryInterface(Components.interfaces.nsIRDFResource);

      this.addMenuItem(aMenuPopup, this.readRDFString(rdfDataSource, charset, kNC_Name), charset.Value);
      if (charset.Value == aPreferenceValue)
        aMenuPopup.parentNode.value = charset.Value;
    }         
  },
  
  mCharsetMenuInitialized: false,
  readDefaultCharset: function()
  {
    if (!this.mCharsetMenuInitialized) 
    {
      Components.classes["@mozilla.org/observer-service;1"]
                .getService(Components.interfaces.nsIObserverService)
                .notifyObservers(null, "charsetmenu-selected", "mailedit");
      // build the charset menu list. We do this by hand instead of using the xul template
      // builder because of Bug #285076, 
      this.createCharsetMenus(document.getElementById("sendDefaultCharset-menupopup"), "NC:MaileditCharsetMenuRoot",
                              document.getElementById('mailnews.send_default_charset').value);
      this.mCharsetMenuInitialized = true;
    }
    return undefined;
  }
};

function addTagCallback(aName, aColor)
{
  var tagService = Components.classes["@mozilla.org/messenger/tagservice;1"].getService(Components.interfaces.nsIMsgTagService);
  tagService.addTag(aName, aColor);
 
  var item = gDisplayPane.appendTagItem(aName, tagService.getKeyForTag(aName), aColor);
  var tagListBox = document.getElementById('tagList');
  tagListBox.ensureElementIsVisible(item);
  tagListBox.selectItem(item);
  tagListBox.focus();
}
