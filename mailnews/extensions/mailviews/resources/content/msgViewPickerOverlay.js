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
 */

const kPersonalAddressbookURI = "moz-abmdbdirectory://abook.mab";

var gOnLoadCalled = false; 
var gMailViewList = null;
var gLastDefaultViewIndex = 8;
var gCurrentViewValue = "";

var nsMsgSearchScope = Components.interfaces.nsMsgSearchScope;
var nsMsgSearchAttrib = Components.interfaces.nsMsgSearchAttrib;
var nsMsgSearchOp = Components.interfaces.nsMsgSearchOp;

// when the item in the list box changes....

function viewChange (aMenuList)
{
  var val = aMenuList.value;

  GetSearchInput();
  gSearchInput.value = "";  // null out any quick search text

  if (val == gCurrentViewValue && val != "7") // don't skip selection of custom view item
    return; 
  gCurrentViewValue = val;

  switch (val)
  {
   case "0": // View All
     gDefaultSearchViewTerms = null;
     onClearSearch(); 
     break;
   case "1": // Unread
     ViewNewMail();
     break;
   case "2":
   case "3":
   case "4":
   case "5":
   case "6":
     ViewLabel(parseInt(val) - 1);
     break;
   case "7":
     LaunchCustomizeDialog();
     break;
   default:
     LoadCustomMailView(parseInt(val) - gLastDefaultViewIndex);
     break;
  } //      
}

function viewPickerOnLoad()
{
  if (gOnLoadCalled) return;

  // hide the advanced search button
  var searchButton = document.getElementById('advancedButton');
  if (searchButton)
  {
    searchButton.setAttribute('collapsed', true);
    FillLabelValues();
    gOnLoadCalled = true;

    refreshCustomMailViews(-1);
  }
}

function LaunchCustomizeDialog()
{
  OpenOrFocusWindow({onOkCallback: refreshCustomMailViews}, 'mailnews:mailviewlist', 'chrome://messenger/content/mailViewList.xul');
  // window.openDialog('chrome://messenger/content/mailViewList.xul', "", 'centerscreen,resizeable,titlebar,chrome', {onOkCallback: refreshCustomMailViews});
}

function LoadCustomMailView(index)
{
  prepareForViewChange();

  var searchTermsArray =  gMailViewList.getMailViewAt(index).searchTerms;
  
  // mark the first node as a group
  var numEntries = searchTermsArray.Count();
  var searchTerm; 
  if (searchTermsArray.Count() > 1)
  {
    searchTerm = searchTermsArray.GetElementAt(0).QueryInterface(Components.interfaces.nsIMsgSearchTerm);
    searchTerm.beginsGrouping = true; 
    searchTerm.booleanAnd = true; // turn the first term to true to work with quick search...
    searchTermsArray.GetElementAt(numEntries - 1).QueryInterface(Components.interfaces.nsIMsgSearchTerm).endsGrouping = true;
  }

  onSearch(searchTermsArray);   
  gDefaultSearchViewTerms = searchTermsArray;
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
    item = menupopupNode.insertBefore(newMenuItem, customNode);
    item.setAttribute('value',  gLastDefaultViewIndex + index);
  }

  if (!numItems)
    customNode.setAttribute('collapsed', true);
  else
    customNode.removeAttribute('collapsed');

  if (aDefaultSelectedIndex >= 0)
  {
    var viewPicker = document.getElementById('viewPicker');
    viewPicker.value = gLastDefaultViewIndex + aDefaultSelectedIndex;
    gCurrentViewValue = viewPicker.value;
    LoadCustomMailView(aDefaultSelectedIndex);
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
  prefString = gPrefs.getComplexValue("mailnews.labels.description." + labelID,  Components.interfaces.nsIPrefLocalizedString);
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
  onSearch(searchTermsArray);  
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
  onSearch(searchTermsArray); 
  
  gDefaultSearchViewTerms = searchTermsArray;
}

function UpdateView()
{
  viewChange(document.getElementById('viewPicker'));
}
