/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2002 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributors:
 *   Scott MacGregor <mscott@netscape.com>
 *   Seth Spitzer <sspitzer@netscape.com>
 */

const kLabelOffset = 1;  // 1=2-1, from msgViewPickerOveraly.xul, <menuitem value="2" id="labelMenuItem1"/>
const kLastDefaultViewIndex = 8;  // 8, because 7 + 1, <menuitem id="createCustomView" value="7" label="&viewPickerCustomView.label;"/>
const kCustomItemValue = "7"; // from msgViewPickerOveraly.xul, <menuitem id="createCustomView" value="7" label="&viewPickerCustomView.label;"/>

var gMailViewList = null;
var gCurrentViewValue = "0"; // initialize to the first view ("All")

var nsMsgSearchScope = Components.interfaces.nsMsgSearchScope;
var nsMsgSearchAttrib = Components.interfaces.nsMsgSearchAttrib;
var nsMsgSearchOp = Components.interfaces.nsMsgSearchOp;

// when the item in the list box changes....

function viewChange(aMenuList)
{
  var val = aMenuList.value;

  if (val == kCustomItemValue) { 
    // restore to the previous view value, in case they cancel
    aMenuList.value = gCurrentViewValue;
    LaunchCustomizeDialog();
    return;
  }

  // bail out early if the user picked the same view
  if (val == gCurrentViewValue)
    return; 
  gCurrentViewValue = val;

  switch (val)
  {
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
   default:
     LoadCustomMailView(parseInt(val) - kLastDefaultViewIndex);
     break;
  } //      

  // store this, to persist across sessions
  gPrefBranch.setIntPref("mailnews.view.last", parseInt(val));

  gQSViewIsDirty = true;
  onEnterInSearchBar();
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

  var searchTermsArray = gMailViewList.getMailViewAt(index).searchTerms;
  
  // create a temporary isupports array to store our search terms
  // since we will be modifying the terms so they work with quick search
  // and we don't want to write out the modified terms when we save the custom view
  // (see bug #189890)
  var searchTermsArrayForQS = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
  
  var numEntries = searchTermsArray.Count();
  for (var i = 0; i < numEntries; i++) {
    var searchTerm = searchTermsArray.GetElementAt(i).QueryInterface(Components.interfaces.nsIMsgSearchTerm); 

    // clone the term, since we might be modifying it
    var searchTermForQS = gSearchSession.createTerm();
    searchTermForQS.value = searchTerm.value;
    searchTermForQS.attrib = searchTerm.attrib;
    searchTermForQS.op = searchTerm.op;

    // mark the first node as a group
    if (i == 0)
      searchTermForQS.beginsGrouping = true;
    else if (i == numEntries - 1)
      searchTermForQS.endsGrouping = true;

    // turn the first term to true to work with quick search...
    searchTermForQS.booleanAnd = i ? searchTerm.booleanAnd : true; 
    
    searchTermsArrayForQS.AppendElement(searchTermForQS);
  }

  createSearchTermsWithList(searchTermsArrayForQS);
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
  viewPicker.selectedItem = viewPicker.getElementsByAttribute("value", aValue)[0];
  viewChange(viewPicker);
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
  createSearchTermsWithList(searchTermsArray);
  gDefaultSearchViewTerms = searchTermsArray;
}

window.addEventListener("load", viewPickerOnLoad, false);
