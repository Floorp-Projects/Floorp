/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sean Su <ssu@netscape.com>
 *   Ian Neal <bugzilla@arlen.demon.co.uk>
 *   Karsten Düsterloh <mnyromyr@tprac.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var gTagListBox = null;

function Startup()
{
  gTagListBox = document.getElementById('tagList');    
  BuildTagList();
}

function GetCSSValue(aElement, aProperty)
{
  return getComputedStyle(aElement, null).getPropertyCSSValue(aProperty).cssText;
}

// appends the tag to the tag list box
function AppendTagItem(aTagName, aKey, aColor)
{
  var item = gTagListBox.appendItem(aTagName, aKey);
  item.style.color = aColor;
  var listBackColor = GetCSSValue(gTagListBox, "background-color");
  var itemForeColor = GetCSSValue(item, "color");
  if (listBackColor == itemForeColor)
    item.style.color = GetCSSValue(gTagListBox, "color");
  return item;
}
 
function BuildTagList()
{
  var tagService = Components.classes["@mozilla.org/messenger/tagservice;1"]
                             .getService(Components.interfaces.nsIMsgTagService);
  var allTags = tagService.tagEnumerator;
  var allKeys = tagService.keyEnumerator;
  while (allTags.hasMore())
  {
    var key = allKeys.getNext();
    AppendTagItem(allTags.getNext(), key, tagService.getColorForKey(key));
  }
}

function DeleteTag()
{
  var tagItemToRemove = gTagListBox.getSelectedItem();
  var index = gTagListBox.selectedIndex;
  if (index >= 0)
  {
    var itemToRemove = gTagListBox.getItemAtIndex(index);
    var tagService = Components.classes["@mozilla.org/messenger/tagservice;1"]
                               .getService(Components.interfaces.nsIMsgTagService);
    tagService.deleteKey(itemToRemove.value);
    gTagListBox.removeItemAt(index);
    var numItemsInListBox = gTagListBox.getRowCount();
    gTagListBox.selectedIndex = index < numItemsInListBox ? index : numItemsInListBox - 1;
  }
}

function AddTag()
{  
  var args = {result: "", okCallback: AddTagCallback};
  var dialog = window.openDialog("chrome://messenger/content/newTagDialog.xul",
                                 "",
                                 "chrome,titlebar,modal",
                                 args);
}
 
function AddTagCallback(aName, aColor)
{
  var tagService = Components.classes["@mozilla.org/messenger/tagservice;1"]
                             .getService(Components.interfaces.nsIMsgTagService);
  tagService.addTag(aName, aColor);
 
  var item = AppendTagItem(aName, tagService.getKeyForTag(aName), aColor);
  var tagListBox = document.getElementById('tagList');
  tagListBox.ensureElementIsVisible(item);
  tagListBox.selectItem(item);
  tagListBox.focus();
}

function RestoreDefaults()
{
  var tagService = Components.classes["@mozilla.org/messenger/tagservice;1"]
                             .getService(Components.interfaces.nsIMsgTagService);
  // remove all existing labels
  var allKeys = tagService.keyEnumerator;
  while (allKeys.hasMore())
  {
    tagService.deleteKey(allKeys.getNext());
    gTagListBox.removeItemAt(0);
  }
  // add default items
  var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefService);
  var prefDescription = prefService.getDefaultBranch("mailnews.labels.description.");
  var prefColor       = prefService.getDefaultBranch("mailnews.labels.color.");
  for (var i = 1; i <= 5; ++i)
  {
    // mimic nsMsgTagService::MigrateLabelsToTags() and create default tags from
    // the former label defaults
    var tag   = prefDescription.getComplexValue(i, Components.interfaces.nsIPrefLocalizedString).data;
    var color = prefColor.getCharPref(i);
    tagService.addTagForKey("$label" + i, tag, color);
  }
  BuildTagList();
}
