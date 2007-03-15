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
    var tagService = Components.classes["@mozilla.org/messenger/tagservice;1"]
                               .getService(Components.interfaces.nsIMsgTagService);
    var tagArray = tagService.getAllTags({});
    for (var i = 0; i < tagArray.length; ++i)
    {
      var taginfo = tagArray[i];
      this.appendTagItem(taginfo.tag, taginfo.key, taginfo.color);
    }
  },
  
  removeTag: function()
  {
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
  
  /** 
   * Open the edit tag dialog
   */
  editTag: function()
  {
    var index = this.mTagListBox.selectedIndex;
    if (index >= 0)
    {
      var tagElToEdit = this.mTagListBox.getItemAtIndex(index);
      var args = {result: "", keyToEdit: tagElToEdit.getAttribute("value"), okCallback: editTagCallback};
      var dialog = window.openDialog(
                  "chrome://messenger/content/newTagDialog.xul",
                  "",
                  "chrome,titlebar,modal",
                  args);
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
  }
};

function addTagCallback(aName, aColor)
{
  var tagService = Components.classes["@mozilla.org/messenger/tagservice;1"]
                    .getService(Components.interfaces.nsIMsgTagService);
  tagService.addTag(aName, aColor, '');
 
  var item = gDisplayPane.appendTagItem(aName, tagService.getKeyForTag(aName), aColor);
  var tagListBox = document.getElementById('tagList');
  tagListBox.ensureElementIsVisible(item);
  tagListBox.selectItem(item);
  tagListBox.focus();
}

function editTagCallback()
{
  // update the values of the selected item
  var tagListEl = document.getElementById('tagList');
  var index = tagListEl.selectedIndex;
  if (index >= 0)
  {
    var tagElToEdit = tagListEl.getItemAtIndex(index);
    var key = tagElToEdit.getAttribute("value");
    var tagService = Components.classes["@mozilla.org/messenger/tagservice;1"]
                     .getService(Components.interfaces.nsIMsgTagService);
    // update the color and label elements
    tagElToEdit.setAttribute("label", tagService.getTagForKey(key));
    tagElToEdit.style.color = tagService.getColorForKey(key);
  }        
}
