/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is the virtual folder properties dialog
 *
 *
 * Contributor(s):
 *  David Bienvenu <bienvenu@nventure.com> 
 *  Scott MacGregor <mscott@mozilla.org>
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

var gFolderPickerTree = null;

function onLoad()
{
  gFolderPickerTree = document.getElementById("folderPickerTree");

  if (window.arguments[0].searchFolderURIs)
  {
    // for each folder uri, 
    var srchFolderUriArray = window.arguments[0].searchFolderURIs.split('|');
    // get the folder for each search URI and set the searchThisFolder flag on it
    for (var i in srchFolderUriArray) 
    {
      var realFolderRes = GetResourceFromUri(srchFolderUriArray[i]);
      var realFolder = realFolderRes.QueryInterface(Components.interfaces.nsIMsgFolder);
      if (realFolder)
        realFolder.setInVFEditSearchScope(true, false);
    }
  }
}

function onUnLoad()
{
  // generateFoldersToSearchList();
}

function onOK()
{
  if ( window.arguments[0].okCallback )
    window.arguments[0].okCallback(generateFoldersToSearchList());
}

function onCancel()
{
  // generateFoldersToSearchList will undo all of our folder property changes for us
  generateFoldersToSearchList(); // ignore return string 
}

function addFolderToSearchListString(aFolder, aCurrentSearchURIString)
{
  if (aCurrentSearchURIString)
    aCurrentSearchURIString += '|';
  aCurrentSearchURIString += aFolder.URI;

  return aCurrentSearchURIString;
}

function processSearchSettingForFolder(aFolder, aCurrentSearchURIString)
{
  if (aFolder.inVFEditSearchScope)
    aCurrentSearchURIString = addFolderToSearchListString(aFolder, aCurrentSearchURIString);
  
  aFolder.setInVFEditSearchScope(false, false);
  return aCurrentSearchURIString;
}

// warning: this routine also clears out the search property list from all of the msg folders
function generateFoldersToSearchList()
{
  var uriSearchString = "";
  var accountManager = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);
  var allServers = accountManager.allServers;
  var numServers = allServers.Count();
  for (var index = 0; index < numServers; index++)
  {
    var rootFolder  = allServers.GetElementAt(index).QueryInterface(Components.interfaces.nsIMsgIncomingServer).rootFolder;
    if (rootFolder)
    {
      uriSearchString = processSearchSettingForFolder(rootFolder, uriSearchString);

      var allFolders = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);
      rootFolder.ListDescendents(allFolders);
      var numFolders = allFolders.Count();
      for (var folderIndex = 0; folderIndex < numFolders; folderIndex++)
        uriSearchString = processSearchSettingForFolder(allFolders.GetElementAt(folderIndex).QueryInterface(Components.interfaces.nsIMsgFolder), uriSearchString);
    }
  } // for each account

  return uriSearchString;
}

function ReverseStateFromNode(row)
{
  var folder = GetFolderResource(row).QueryInterface(Components.interfaces.nsIMsgFolder);
  var currentState = folder.inVFEditSearchScope;

  folder.setInVFEditSearchScope(!currentState, false);
}

function GetFolderResource(rowIndex)
{
  return gFolderPickerTree.builder.QueryInterface(Components.interfaces.nsIXULTreeBuilder).getResourceAtIndex(rowIndex);
}

function selectFolderTreeOnClick(event)
{
  // we only care about button 0 (left click) events
  if (event.button != 0 || event.originalTarget.localName != "treechildren")
   return;
 
  var row = {}, col = {}, obj = {};
  gFolderPickerTree.treeBoxObject.getCellAt(event.clientX, event.clientY, row, col, obj);
  if (row.value == -1 || row.value > (gFolderPickerTree.view.rowCount - 1))
    return;

  if (event.detail == 2) {
    // only toggle the search folder state when double clicking something
    // that isn't a container
    if (!gFolderPickerTree.view.isContainer(row.value)) {
      ReverseStateFromNode(row.value);
      return;
    } 
  }
  else if (event.detail == 1)
  {
    if (obj.value != "twisty" && col.value == "selectedColumn")
      ReverseStateFromNode(row.value)
  }
}

function onSelectFolderTreeKeyPress(event)
{
  // for now, only do something on space key
  if (event.charCode != KeyEvent.DOM_VK_SPACE)
    return;

  var treeSelection = gFolderPickerTree.view.selection; 
  for (var i=0;i<treeSelection.getRangeCount();i++) {
    var start = {}, end = {};
    treeSelection.getRangeAt(i,start,end);
    for (var k=start.value;k<=end.value;k++)
      ReverseStateFromNode(k);
  }
}

