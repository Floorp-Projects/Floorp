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

var nsMsgSearchScope = Components.interfaces.nsMsgSearchScope;

var gDialog;
var gMailView = null;
var searchSessionContractID = "@mozilla.org/messenger/searchSession;1";
var msgWindow; // important, don't change the name of this variable. it's really a global used by commandglue.js 
var gSearchTermSession; // really an in memory temporary filter we use to read in and write out the search terms
var gSearchFolderURIs = "";

function onLoad()
{
  var arguments = window.arguments[0];

  gDialog = {};

  gDialog.OKButton = document.documentElement.getButton("accept");

  gDialog.nameField = document.getElementById("name");
  gDialog.nameField.focus();

  // call this when OK is pressed
  msgWindow = arguments.msgWindow;
 
  // pre select the folderPicker, based on what they selected in the folder pane
  gDialog.picker = document.getElementById("msgNewFolderPicker");
  MsgFolderPickerOnLoad("msgNewFolderPicker");
 
  initializeSearchWidgets();
  setSearchScope(nsMsgSearchScope.offlineMail);  

  if (arguments.editExistingFolder)
    InitDialogWithVirtualFolder(arguments.preselectedURI);
  else // we are creating a new virtual folder
  {
    // it is possible that we were given arguments to pre-fill the dialog with...
    gSearchTermSession = Components.classes[searchSessionContractID].createInstance(Components.interfaces.nsIMsgSearchSession);

    if (arguments.searchTerms) // then add them to our search session
    {
      var count = arguments.searchTerms.Count();
      for (var searchIndex = 0; searchIndex < count; )
        gSearchTermSession.appendTerm(arguments.searchTerms.QueryElementAt(searchIndex++, Components.interfaces.nsIMsgSearchTerm));
    }
    if (arguments.preselectedURI)
    {
      gSearchFolderURIs = arguments.preselectedURI;
      var folderToSearch = GetMsgFolderFromUri(arguments.preselectedURI, false);
      SetFolderPicker(folderToSearch.parent ? folderToSearch.parent.URI : gSearchFolderURIs, "msgNewFolderPicker");
    }
    if (arguments.newFolderName) 
      document.getElementById("name").value = arguments.newFolderName;
    if (arguments.searchFolderURIs)
      gSearchFolderURIs = arguments.searchFolderURIs;

    setupSearchRows(gSearchTermSession.searchTerms);
    doEnabling(); // we only need to disable/enable the OK button for new virtual folders
  }
  
  updateOnlineSearchState();
  doSetOKCancel(onOK, onCancel);
}

function setupSearchRows(aSearchTerms)
{
  if (aSearchTerms && aSearchTerms.Count() > 0)
  {
    // load the search terms for the folder
    initializeSearchRows(nsMsgSearchScope.offlineMail, aSearchTerms);
    gSearchLessButton.removeAttribute("disabled", "false");
  }
  else
    onMore(null);
}

function updateOnlineSearchState()
{
  var enableCheckbox = false;
  var checkbox = document.getElementById('searchOnline');
  // only enable the checkbox for selection, for online servers
  var srchFolderUriArray = gSearchFolderURIs.split('|');
  if (srchFolderUriArray[0])
  {
    var realFolderRes = GetResourceFromUri(srchFolderUriArray[0]);
    var realFolder = realFolderRes.QueryInterface(Components.interfaces.nsIMsgFolder);
    enableCheckbox =  realFolder.server.offlineSupportLevel; // anything greater than 0 is an online server like IMAP or news
  }

  if (enableCheckbox)
    checkbox.removeAttribute('disabled');
  else
  {
    checkbox.setAttribute('disabled', true);
    checkbox.checked = false;
  }
}

function InitDialogWithVirtualFolder(aVirtualFolderURI)
{
  // when editing an existing folder, hide the folder picker that stores the parent location of the folder
  document.getElementById("chooseFolderLocationRow").collapsed = true;
  var folderNameField = document.getElementById("name");
  folderNameField.disabled = true;

  var msgFolder = GetMsgFolderFromUri(aVirtualFolderURI);
  var msgDatabase = msgFolder.getMsgDatabase(msgWindow);
  var dbFolderInfo = msgDatabase.dBFolderInfo;
  
  gSearchFolderURIs = dbFolderInfo.getCharPtrProperty("searchFolderUri");
  var searchTermString = dbFolderInfo.getCharPtrProperty("searchStr");
  document.getElementById('searchOnline').checked = dbFolderInfo.getBooleanProperty("searchOnline", false);
  
  // work around to get our search term string converted into a real array of search terms
  var filterService = Components.classes["@mozilla.org/messenger/services/filters;1"].getService(Components.interfaces.nsIMsgFilterService);
  var filterList = filterService.getTempFilterList(msgFolder);
  gSearchTermSession = filterList.createFilter("temp");
  filterList.parseCondition(gSearchTermSession, searchTermString);

  setupSearchRows(gSearchTermSession.searchTerms);

  // set the name of the folder
  folderNameField.value = msgFolder.prettyName;

  // update the window title based on the name of the saved search
  var messengerBundle = document.getElementById("bundle_messenger");
  document.getElementById('virtualFolderPropertiesDialog').setAttribute('title',messengerBundle.getFormattedString('editVirtualFolderPropertiesTitle', [msgFolder.prettyName]));
}

function onOK()
{
  var name = gDialog.nameField.value;
  var uri = gDialog.picker.getAttribute("uri");
  var messengerBundle = document.getElementById("bundle_messenger");
  var searchOnline = document.getElementById('searchOnline').checked;

  if (!gSearchFolderURIs)
  {  
    window.alert(messengerBundle.getString('alertNoSearchFoldersSelected'));
    return false;
  }

  if (window.arguments[0].editExistingFolder)
  {
    // update the search terms 
    saveSearchTerms(gSearchTermSession.searchTerms, gSearchTermSession);
    var searchTermString = getSearchTermString(gSearchTermSession.searchTerms);
     
    var msgFolder = GetMsgFolderFromUri(window.arguments[0].preselectedURI);
    var msgDatabase = msgFolder.getMsgDatabase(msgWindow);
    var dbFolderInfo = msgDatabase.dBFolderInfo;

    // set the view string as a property of the db folder info
    // set the original folder name as well.
    dbFolderInfo.setCharPtrProperty("searchStr", searchTermString);
    dbFolderInfo.setCharPtrProperty("searchFolderUri", gSearchFolderURIs);
    dbFolderInfo.setBooleanProperty("searchOnline", searchOnline);    
    msgDatabase.Close(true);

    var accountManager = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);
    accountManager.saveVirtualFolders();

    if (window.arguments[0].onOKCallback)
      window.arguments[0].onOKCallback(msgFolder.URI);
      
  } 
  else if (name && uri) // create a new virtual folder
  {
    
    // check to see if we already have a folder with the same name and alert the user if so...
    var parentFolder = GetMsgFolderFromUri(uri);
    if (parentFolder.containsChildNamed(name))
    {
      window.alert(messengerBundle.getString('folderExists'));
      return false;      
    }

    // XXX: Add code to make sure a folder with this name does not already exist before creating the virtual folder...
    // Alert the user here if that is the case.
    saveSearchTerms(gSearchTermSession.searchTerms, gSearchTermSession);
    CreateVirtualFolder(name, parentFolder, gSearchFolderURIs, gSearchTermSession.searchTerms, searchOnline);
  }

  return true;
}

function onCancel()
{
  // close the window
  return true;
}

function doEnabling()
{
  if (gDialog.nameField.value && gDialog.picker.getAttribute("uri")) 
  {
    if (gDialog.OKButton.disabled)
      gDialog.OKButton.disabled = false;
  } 
  else
    gDialog.OKButton.disabled = true;
}

function chooseFoldersToSearch()
{
  // if we have some search folders already, then root the folder picker dialog off the account
  // for those folders. Otherwise fall back to the preselectedfolderURI which is the parent folder
  // for this new virtual folder.
  var srchFolderUriArray = gSearchFolderURIs.split('|');    
  var folder  = GetMsgFolderFromUri(srchFolderUriArray[0] ? srchFolderUriArray[0] : window.arguments[0].preselectedURI, false);
  var dialog = window.openDialog("chrome://messenger/content/virtualFolderListDialog.xul", "",
                                 "chrome,titlebar,modal,centerscreen,resizable",
                                 {serverURI:folder.rootFolder.URI,
                                  searchFolderURIs:gSearchFolderURIs,
                                  okCallback:onFolderListDialogCallback}); 
}

// callback routine from chooseFoldersToSearch
function onFolderListDialogCallback(searchFolderURIs)
{
  gSearchFolderURIs = searchFolderURIs;
  updateOnlineSearchState(); // we may have changed the server type we are searching...
}

function onEnterInSearchTerm()
{
  // stub function called by the core search widget code...
  // nothing for us to do here
}
