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

const kLabelOffset = 1;  // 1=2-1, from msgViewPickerOveraly.xul, <menuitem value="2" id="labelMenuItem1"/>
const kLastDefaultViewIndex = 9;  // 9, because 8 + 1, <menuitem id="createCustomView" value="7" label="&viewPickerCustomView.label;"/>
const kCustomItemValue = "8"; // from msgViewPickerOveraly.xul, <menuitem id="createCustomView" value="7" label="&viewPickerCustomView.label;"/>

var gMailViewList = null;
var gCurrentViewValue = "0"; // initialize to the first view ("All")
var gCurrentViewLabel = "";
var gSaveDefaultSVTerms;

var nsMsgSearchScope = Components.interfaces.nsMsgSearchScope;
var nsMsgSearchAttrib = Components.interfaces.nsMsgSearchAttrib;
var nsMsgSearchOp = Components.interfaces.nsMsgSearchOp;

// when the item in the list box changes....

function viewChange(aMenuList, val)
{
  if (val == kCustomItemValue) { 
    // restore to the previous view value, in case they cancel
    aMenuList.value = gCurrentViewValue;
    LaunchCustomizeDialog();
    return;
  }

  // bail out early if the user picked the same view
  if (val == gCurrentViewValue)
    return; 
  viewDebug("viewChange to " + val + "\n");
  var oldViewValue = gCurrentViewValue;
  gCurrentViewValue = val;
  switch (val)
  {
   case -1:
   case "-1":
   case "0": // View All
     gDefaultSearchViewTerms = null;
     break;
   case "1": // Unread
     ViewNewMail();
     break;
   case "2": // label 1
   case "3": // label 2
   case "4": // label 3
   case "5": // label 4
   case "6": // label 5
     ViewLabel(parseInt(val) - kLabelOffset);
     break;
   case "7": // save view as virtual folder
     getViewName(CreateVFFromView, gCurrentViewLabel);
     val = oldViewValue;
     aMenuList.value = val;
     break;
   default:
     LoadCustomMailView(parseInt(val) - kLastDefaultViewIndex);
     break;
  } //      

  gSaveDefaultSVTerms = gDefaultSearchViewTerms;
  // store this, to persist across sessions
  if (val != "-1" && val != -1)
  {
    if (aMenuList.selectedItem)
      gCurrentViewLabel = aMenuList.selectedItem.label;
    gPrefBranch.setIntPref("mailnews.view.last", parseInt(val));  
  }
  // if we're switching to -1 (virtual folder), don't do a search 
  if (val != "-1" && val != -1)
  {
    onEnterInSearchBar();
    gQSViewIsDirty = true;
  }
}

function CreateVFFromView(newName, origFolderURI)
{
  var selectedFolder = GetResourceFromUri(origFolderURI);
  var folderToSearch = selectedFolder.QueryInterface(Components.interfaces.nsIMsgFolder);

  CreateVirtualFolder(newName, folderToSearch.parent, origFolderURI, gSaveDefaultSVTerms);
}

const kLabelPrefs = "mailnews.labels.description.";

const gLabelPrefListener = {
  observe: function(subject, topic, prefName)
  {
    if (topic != "nsPref:changed")
      return;

    var index = parseInt(prefName.substring(kLabelPrefs.length));
    if (index >= 1 && index <= 5)
      setLabelAttributes(index, "labelMenuItem" + index);
  }
};

function AddLabelPrefListener()
{
  try {
    gPrefBranch.QueryInterface(Components.interfaces.nsIPrefBranchInternal);
    gPrefBranch.addObserver(kLabelPrefs, gLabelPrefListener, false);
  } catch(ex) {
    dump("Failed to observe prefs: " + ex + "\n");
  }
}

function RemoveLabelPrefListener()
{
  try {
    gPrefBranch.QueryInterface(Components.interfaces.nsIPrefBranchInternal);
    gPrefBranch.removeObserver(kLabelPrefs, gLabelPrefListener);
  } catch(ex) {
    dump("Failed to remove pref observer: " + ex + "\n");
  }
}

function viewPickerOnLoad()
{
  if (document.getElementById('viewPicker')) {
    window.addEventListener("unload", RemoveLabelPrefListener, false);
    
    AddLabelPrefListener();

    FillLabelValues();

    refreshCustomMailViews(-1);
  }
}

function LaunchCustomizeDialog()
{
  // made it modal, see bug #191188
  window.openDialog("chrome://messenger/content/mailViewList.xul", "mailnews:mailviewlist", "chrome,modal,titlebar,resizable,centerscreen", {onCloseCallback: refreshCustomMailViews});
}

function LoadCustomMailView(index)
{
  prepareForViewChange();

  var searchTermsArrayForQS = CreateGroupedSearchTerms(gMailViewList.getMailViewAt(index).searchTerms);
  createSearchTermsWithList(searchTermsArrayForQS);
  AddVirtualFolderTerms(searchTermsArrayForQS);
  gDefaultSearchViewTerms = searchTermsArrayForQS;
}

function refreshCustomMailViews(aDefaultSelectedIndex)
{
  // for each mail view in the msg view list, add an entry in our combo box
  if (!gMailViewList)
    gMailViewList = Components.classes["@mozilla.org/messenger/mailviewlist;1"].getService(Components.interfaces.nsIMsgMailViewList);

  // remove any existing entries...
  var menupopupNode = document.getElementById('viewPickerPopup');
  var numItemsInNode = menupopupNode.childNodes.length;
  numItemsInNode -= 2; // subtract off the last 2 items since we want to leave those
  var removeNodes = true;

  while (removeNodes && numItemsInNode)
  {
    if (menupopupNode.childNodes[numItemsInNode-1].getAttribute('id') == 'lastDefaultView')
      removeNodes = false;
    else
      menupopupNode.removeChild(menupopupNode.childNodes[numItemsInNode-1]);
    numItemsInNode -= 1;
  }
 
  // now rebuild the list

  var numItems = gMailViewList.mailViewCount; 
  var customNode = document.getElementById('createCustomViewSeparator');
  var newMenuItem; 
  var item; 
  for (var index = 0; index < numItems; index++)
  {
    newMenuItem = document.createElement('menuitem');
    newMenuItem.setAttribute('label', gMailViewList.getMailViewAt(index).mailViewName);
    newMenuItem.setAttribute('id', "userdefinedview" + (kLastDefaultViewIndex + index));
    item = menupopupNode.insertBefore(newMenuItem, customNode);
    item.setAttribute('value',  kLastDefaultViewIndex + index);
  }

  if (!numItems)
    customNode.setAttribute('collapsed', true);
  else
    customNode.removeAttribute('collapsed');

  if (aDefaultSelectedIndex >= 0)
  {
    ViewChangeByValue(kLastDefaultViewIndex + aDefaultSelectedIndex);
  }
}

function ViewChangeByValue(aValue)
{
  var viewPicker = document.getElementById('viewPicker');
  if (aValue == -1)
  {
    viewPicker.selectedItem = null;
    viewChange(viewPicker, -1);
  }
  else
  {
    viewPicker.selectedItem = viewPicker.getElementsByAttribute("value", aValue)[0];
    viewChange(viewPicker, viewPicker.value);
  }
}

function FillLabelValues()
{
  for (var i = 1; i <= 5; i++)
    setLabelAttributes(i, "labelMenuItem" + i);
}

function setLabelAttributes(labelID, menuItemID)
{
  var prefString;
  prefString = gPrefBranch.getComplexValue(kLabelPrefs + labelID, Components.interfaces.nsIPrefLocalizedString).data;
  document.getElementById(menuItemID).setAttribute("label", prefString);
}

function prepareForViewChange()
{
  // this is a problem - it saves the current view in gPreQuickSearchView
  // then we eventually call onEnterInSearchBar, and we think we need to restore the pre search view!
  initializeSearchBar();
  ClearThreadPaneSelection();
  ClearMessagePane();
}

function ViewLabel(labelID)
{
  prepareForViewChange();

  // create an i supports array to store our search terms 
  var searchTermsArray = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
  var term = gSearchSession.createTerm();
  var value = term.value;

  value.label = labelID;
  value.attrib = nsMsgSearchAttrib.Label;
  term.value = value;
  term.attrib = nsMsgSearchAttrib.Label;
  term.op = nsMsgSearchOp.Is;
  term.booleanAnd = true;

  searchTermsArray.AppendElement(term);
  AddVirtualFolderTerms(searchTermsArray);
  createSearchTermsWithList(searchTermsArray);
  gDefaultSearchViewTerms = searchTermsArray;
}

function ViewNewMail()
{
  prepareForViewChange();

  // create an i supports array to store our search terms 
  var searchTermsArray = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);

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
    for (i = 0; i < termsArray.Count(); i++)
    {
      isupports = termsArray.GetElementAt(i);
      searchTerm = isupports.QueryInterface(Components.interfaces.nsIMsgSearchTerm);
      searchTermsArray.AppendElement(searchTerm);
    }
  }
}


window.addEventListener("load", viewPickerOnLoad, false);
