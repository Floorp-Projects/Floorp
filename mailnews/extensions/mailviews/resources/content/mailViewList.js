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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2002 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
 */


var gMailListView; 
var gListBox; 
var gOkCallback = null; 
var gCancelCallback = null;
var gEditButton;
var gDeleteButton;

function mailViewListOnLoad()
{
  gMailListView = Components.classes["@mozilla.org/messenger/mailviewlist;1"].getService(Components.interfaces.nsIMsgMailViewList);; 
  gListBox = document.getElementById('mailViewList');

  if ("arguments" in window && window.arguments[0]) 
  {
    var args = window.arguments[0];
    if ("onOkCallback" in args)
      gOkCallback =  window.arguments[0].onOkCallback;
    if ("onCancelCallback" in args)
      gCancelCallback =  window.arguments[0].onCancelCallback;
  }

  // Construct list view based on current mail view list data
  refreshListView(null);
  gEditButton = document.getElementById('editButton');
  gDeleteButton = document.getElementById('deleteButton');

  updateButtons();

  doSetOKCancel(onOK, onCancel);
}

function onOK()
{
  if (gOkCallback)
    gOkCallback(gListBox.selectedIndex);
  return true;
}

function onCancel()
{
  if (gCancelCallback)
    gCancelCallback();
  return true;
}

function refreshListView(aSelectedMailView)
{
  // remove any existing items in the view...
  for (var index = gListBox.getRowCount(); index > 0; index--)
  {
    gListBox.removeChild(gListBox.getItemAtIndex(index - 1));
  }

  var numItems = gMailListView.mailViewCount;
  var mailView; 
  for (index = 0; index < numItems; index++)
  {
    mailView = gMailListView.getMailViewAt(index);
    gListBox.appendItem(mailView.mailViewName, index);
    if (aSelectedMailView && (mailView.mailViewName == aSelectedMailView.mailViewName) )
      gListBox.selectedIndex = index;
  }
}

function onNewMailView()
{
   window.openDialog('chrome://messenger/content/mailViewSetup.xul', "", 'centerscreen,resizeable,modal,titlebar,chrome', {onOkCallback: refreshListView});
}

function onDeleteMailView()
{
  // get the selected index
  var selectedIndex = gListBox.selectedIndex;
  if (selectedIndex >= 0)
  {
    var mailView = gMailListView.getMailViewAt(selectedIndex);
    if (mailView)
    {
      gMailListView.removeMailView(mailView);
      // now remove it from the view...
      gListBox.removeChild(gListBox.selectedItem);

      // select the next item in the list..
      if (selectedIndex < gListBox.getRowCount())
        gListBox.selectedIndex = selectedIndex;
      else
        gListBox.selectedIndex = gListBox.getRowCount() - 1;

      gMailListView.save();
    }
  }
}

function onEditMailView()
{
  // get the selected index
  var selectedIndex = gListBox.selectedIndex;
  if (selectedIndex >= 0)
  {
    var selMailView = gMailListView.getMailViewAt(selectedIndex);
    // open up the mail view setup dialog passing in the mail view as an argument....

    var args = {mailView: selMailView, onOkCallback: refreshListView};

    window.openDialog('chrome://messenger/content/mailViewSetup.xul', "", 'centerscreen,modal,resizeable,titlebar,chrome', args);
  }
}

function onMailViewSelect(event)
{
  updateButtons();
}

function updateButtons()
{
  var selectedIndex = gListBox.selectedIndex;
  // "edit" and "delete" only enabled when one filter selected
  gEditButton.disabled = selectedIndex < 0;
  gDeleteButton.disabled = selectedIndex < 0;
}

function doHelpButton()
{
  openHelp("message-views-using");
}

