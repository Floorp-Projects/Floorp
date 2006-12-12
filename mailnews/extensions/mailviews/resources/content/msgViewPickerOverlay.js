/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Karsten Düsterloh <mnyromyr@tprac.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

// menuitem value constants
// tag views have kViewTagMarker + their key as value
const kViewItemAll         = 0;
const kViewItemUnread      = 1;
const kViewItemTags        = 2; // former labels used values 2-6
const kViewItemVirtual     = 7;
const kViewItemCustomize   = 8;
const kViewItemFirstCustom = 9;

const kViewCurrent    = "current-view";
const kViewCurrentTag = "current-view-tag";
const kViewTagMarker  = ":";

var gMailViewList = null;
var gCurrentViewValue = kViewItemAll;
var gCurrentViewLabel = "";
var gSaveDefaultSVTerms;

var nsMsgSearchScope  = Components.interfaces.nsMsgSearchScope;
var nsMsgSearchAttrib = Components.interfaces.nsMsgSearchAttrib;
var nsMsgSearchOp     = Components.interfaces.nsMsgSearchOp;


// perform the view/action requested by the aValue string
// and set the view picker label to the aLabel string
function ViewChange(aValue, aLabel)
{
  if (aValue == kViewItemCustomize || aValue == kViewItemVirtual)
  {
    // restore to the previous view value, in case they cancel
    UpdateViewPicker(gCurrentViewValue, gCurrentViewLabel);
    if (aValue == kViewItemCustomize)
      LaunchCustomizeDialog();
    else
      openNewVirtualFolderDialogWithArgs(gCurrentViewLabel, gSaveDefaultSVTerms);
    return;
  }

  // bail out early if the user picked the same view
  if (gCurrentViewValue == aValue)
    return;

  // persist the view
  gCurrentViewValue = aValue;
  gCurrentViewLabel = aLabel;
  SetMailViewForFolder(GetFirstSelectedMsgFolder(), gCurrentViewValue)
  UpdateViewPicker(gCurrentViewValue, gCurrentViewLabel);

  // tag menuitem values are of the form :<keyword>
  if (isNaN(aValue))
  {
    // split off the tag key
    var tagkey = aValue.substr(kViewTagMarker.length);
    ViewTagKeyword(tagkey);
  }
  else
  {
    var numval = Number(aValue);
    switch (numval)
    {
      case kViewItemAll: // View All
        gDefaultSearchViewTerms = null;
        break;
      case kViewItemUnread: // Unread
        ViewNewMail();
        break;
      default:
        // for legacy reasons, custom views start at index 9
        LoadCustomMailView(numval - kViewItemFirstCustom);
        break;
    }
  }
  gSaveDefaultSVTerms = gDefaultSearchViewTerms;
  onEnterInSearchBar();
  gQSViewIsDirty = true;
}


function ViewChangeByMenuitem(aMenuitem)
{
  // Mac View menu menuitems don't have XBL bindings
  ViewChange(aMenuitem.getAttribute("value"), aMenuitem.getAttribute("label"));
}


function ViewChangeByValue(aValue)
{
  var label = "";
  var viewPickerPopup = document.getElementById("viewPickerPopup");
  if (viewPickerPopup)
  {
    // grab the label for the menulist from one of its menuitems
    var selectedItems = viewPickerPopup.getElementsByAttribute("value", aValue);
    if (!selectedItems || !selectedItems.length)
    {
      // we may have a new item
      RefreshViewPopup(viewPickerPopup, true);
      selectedItems = viewPickerPopup.getElementsByAttribute("value", aValue);
    }
    label = selectedItems && selectedItems.length && selectedItems[0].label;
  }
  ViewChange(aValue, label);
}


function UpdateViewPicker(aValue, aLabel)
{
  var viewPicker = document.getElementById("viewPicker");
  if (viewPicker)
  {
    viewPicker.value = aValue;
    viewPicker.setAttribute("label", aLabel);
  }
}


function GetFolderInfo(aFolder)
{
  if (aFolder)
  {
    var db = aFolder.getMsgDatabase(msgWindow);
    if (db)
      return db.dBFolderInfo;
  }
  return null;
}


function GetMailViewForFolder(aFolder)
{
  var val = "";
  var folderInfo = GetFolderInfo(aFolder);
  if (folderInfo)
  {
    val = folderInfo.getCharPtrProperty(kViewCurrentTag);
    if (!val)
    {
      // no new view value, thus using the old
      var numval = folderInfo.getUint32Property(kViewCurrent, kViewItemAll);
      // and migrate it, if it's a former label view (label views used values 2-6)
      if ((kViewItemTags <= numval) && (numval < kViewItemVirtual))
        val = kViewTagMarker + "$label" + (val - 1);
      else
        val = numval;
    }
  }
  return val;
}


function SetMailViewForFolder(aFolder, aValue)
{
  var folderInfo = GetFolderInfo(aFolder);
  if (folderInfo)
  {
    // we can't map tags back to labels in general,
    // so set view to all for backwards compatibility in this case
    folderInfo.setUint32Property (kViewCurrent, isNaN(aValue) ? kViewItemAll : aValue);
    folderInfo.setCharPtrProperty(kViewCurrentTag, aValue);
  }
}


function LaunchCustomizeDialog()
{
  OpenOrFocusWindow({}, "mailnews:mailviewlist", "chrome://messenger/content/mailViewList.xul");
}


function LoadCustomMailView(index)
{
  PrepareForViewChange();
  var searchTermsArrayForQS = CreateGroupedSearchTerms(gMailViewList.getMailViewAt(index).searchTerms);
  createSearchTermsWithList(searchTermsArrayForQS);
  AddVirtualFolderTerms(searchTermsArrayForQS);
  gDefaultSearchViewTerms = searchTermsArrayForQS;
}


function ViewTagKeyword(keyword)
{
  PrepareForViewChange();

  // create an i supports array to store our search terms
  var searchTermsArray = Components.classes["@mozilla.org/supports-array;1"]
                                   .createInstance(Components.interfaces.nsISupportsArray);
  var term = gSearchSession.createTerm();
  var value = term.value;

  value.str = keyword;
  value.attrib = nsMsgSearchAttrib.Keywords;
  term.value = value;
  term.attrib = nsMsgSearchAttrib.Keywords;
  term.op = nsMsgSearchOp.Contains;
  term.booleanAnd = true;

  searchTermsArray.AppendElement(term);
  AddVirtualFolderTerms(searchTermsArray);
  createSearchTermsWithList(searchTermsArray);
  gDefaultSearchViewTerms = searchTermsArray;
}


function ViewNewMail()
{
  PrepareForViewChange();

  // create an i supports array to store our search terms
  var searchTermsArray = Components.classes["@mozilla.org/supports-array;1"]
                                   .createInstance(Components.interfaces.nsISupportsArray);
  var term = gSearchSession.createTerm();
  var value = term.value;

  value.status = 1;
  value.attrib = nsMsgSearchAttrib.MsgStatus;
  term.value = value;
  term.attrib = nsMsgSearchAttrib.MsgStatus;
  term.op = nsMsgSearchOp.Isnt;
  term.booleanAnd = true;
  searchTermsArray.AppendElement(term);

  AddVirtualFolderTerms(searchTermsArray);

  createSearchTermsWithList(searchTermsArray);
  // not quite right - these want to be just the view terms...but it might not matter.
  gDefaultSearchViewTerms = searchTermsArray;
}


function AddVirtualFolderTerms(searchTermsArray)
{
  // add in any virtual folder terms
  var virtualFolderSearchTerms = (gVirtualFolderTerms || gXFVirtualFolderTerms);
  if (virtualFolderSearchTerms)
  {
    var isupports = null;
    var searchTerm;
    var termsArray = virtualFolderSearchTerms.QueryInterface(Components.interfaces.nsISupportsArray);
    for (var i = 0; i < termsArray.Count(); i++)
    {
      isupports = termsArray.GetElementAt(i);
      searchTerm = isupports.QueryInterface(Components.interfaces.nsIMsgSearchTerm);
      searchTermsArray.AppendElement(searchTerm);
    }
  }
}


function PrepareForViewChange()
{
  // this is a problem - it saves the current view in gPreQuickSearchView
  // then we eventually call onEnterInSearchBar, and we think we need to restore the pre search view!
  initializeSearchBar();
  ClearThreadPaneSelection();
  ClearMessagePane();
}


// recreate the entries for tags and custom views
// and mark the current view's menuitem
function RefreshViewPopup(aViewPopup, aIsMenulist)
{
  var menupopups = aViewPopup.getElementsByTagName("menupopup");
  if (menupopups.length > 1)
  {
    // when we have menupopups, we assume both tags and custom views are there
    RefreshTagsPopup(menupopups[0], aIsMenulist);
    RefreshCustomViewsPopup(menupopups[1], aIsMenulist);
  }
  // mark default views if selected
  if (!aIsMenulist)
  {
    var viewAll = aViewPopup.getElementsByAttribute("value", kViewItemAll)[0];
    viewAll.setAttribute("checked", gCurrentViewValue == kViewItemAll);
    var viewUnread = aViewPopup.getElementsByAttribute("value", kViewItemUnread)[0];
    viewUnread.setAttribute("checked", gCurrentViewValue == kViewItemUnread);
  }
}


function RefreshCustomViewsPopup(aMenupopup, aIsMenulist)
{
  // for each mail view in the msg view list, add an entry in our combo box
  if (!gMailViewList)
    gMailViewList = Components.classes["@mozilla.org/messenger/mailviewlist;1"]
                              .getService(Components.interfaces.nsIMsgMailViewList);
  // remove all menuitems
  while (aMenupopup.hasChildNodes())
    aMenupopup.removeChild(aMenupopup.lastChild);

  // now rebuild the list
  var currentView = isNaN(gCurrentViewValue) ? kViewItemAll : Number(gCurrentViewValue);
  var numItems = gMailViewList.mailViewCount;
  for (var i = 0; i < numItems; ++i)
  {
    var viewInfo = gMailViewList.getMailViewAt(i);
    var menuitem = document.createElement("menuitem");
    menuitem.setAttribute("label", viewInfo.prettyName);
    menuitem.setAttribute("value", kViewItemFirstCustom + i);
    if (!aIsMenulist)
    {
      menuitem.setAttribute("type", "radio");
      if (kViewItemFirstCustom + i == currentView)
        menuitem.setAttribute("checked", true);
    }
    aMenupopup.appendChild(menuitem);
  }
}


function RefreshTagsPopup(aMenupopup, aIsMenulist)
{
  // remove all menuitems
  while (aMenupopup.hasChildNodes())
    aMenupopup.removeChild(aMenupopup.lastChild);

  // create tag menuitems
  var currentTagKey = isNaN(gCurrentViewValue) ? gCurrentViewValue.substr(kViewTagMarker.length) : "";
  var tagService = Components.classes["@mozilla.org/messenger/tagservice;1"]
                             .getService(Components.interfaces.nsIMsgTagService);
  var tagArray = tagService.getAllTags({});
  for (var i = 0; i < tagArray.length; ++i)
  {
    var tagInfo = tagArray[i];
    var menuitem = document.createElement("menuitem");
    menuitem.setAttribute("label", tagInfo.tag);
    menuitem.setAttribute("value", kViewTagMarker + tagInfo.key);
    if (!aIsMenulist)
    {
      menuitem.setAttribute("type", "radio");
      if (tagInfo.key == currentTagKey)
        menuitem.setAttribute("checked", true);
    }
    var color = tagInfo.color;
    if (color)
      menuitem.setAttribute("class", "lc-" + color.substr(1));
    aMenupopup.appendChild(menuitem);
  }
}


function ViewPickerOnLoad()
{
  var viewPickerPopup = document.getElementById("viewPickerPopup");
  if (viewPickerPopup)
    RefreshViewPopup(viewPickerPopup, true);
}


window.addEventListener("load", ViewPickerOnLoad, false);
