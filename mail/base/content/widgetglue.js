# -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation. Portions created by Netscape are
# Copyright (C) 1998-1999 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s):
#   Jan Varga (varga@nixcorp.com)
#   Håkan Waara (hwaara@chello.se)

/*
 * widget-specific wrapper glue. There should be one function for every
 * widget/menu item, which gets some context (like the current selection)
 * and then calls a function/command in commandglue
 */
 
//The eventual goal is for this file to go away and its contents to be brought into
//mailWindowOverlay.js.  This is currently being done.

//NOTE: gMessengerBundle must be defined and set or this Overlay won't work

function ConvertDOMListToResourceArray(nodeList)
{
    var result = Components.classes["@mozilla.org/supports-array;1"].createInstance(Components.interfaces.nsISupportsArray);

    for (var i=0; i<nodeList.length; i++) {
        result.AppendElement(nodeList[i].resource);
    }

    return result;
}

function GetSelectedFolderURI()
{
    var folderTree = GetFolderTree();
    var selection = folderTree.view.selection;
    if (selection.count == 1)
    {
        var startIndex = {};
        var endIndex = {};
        selection.getRangeAt(0, startIndex, endIndex);
        var folderResource = GetFolderResource(folderTree, startIndex.value);
        return folderResource.Value;
    }

    return null;
}

function MsgRenameFolder() 
{
	var preselectedURI = GetSelectedFolderURI();
	var folderTree = GetFolderTree();

	var name = GetFolderNameFromUri(preselectedURI, folderTree);

	dump("preselectedURI = " + preselectedURI + "\n");
	var dialog = window.openDialog(
	             "chrome://messenger/content/renameFolderDialog.xul",
	             "newFolder",
	             "chrome,titlebar,modal",
	             {preselectedURI: preselectedURI, 
	              okCallback: RenameFolder, name: name});
}

function RenameFolder(name,uri)
{
  dump("uri,name = " + uri + "," + name + "\n");
  var folderTree = GetFolderTree();
  if (folderTree)
  {
    if (uri && (uri != "") && name && (name != "")) 
    {
      var selectedFolder = GetResourceFromUri(uri);
      if (gDBView)
        gCurrentlyDisplayedMessage = gDBView.currentlyDisplayedMessage;

      ClearThreadPane();
      ClearMessagePane();
      folderTree.view.selection.clearSelection();

      try
      {
        messenger.RenameFolder(GetFolderDatasource(), selectedFolder, name);
      }
      catch(e)
      {
        SelectFolder(selectedFolder.URI);  //restore selection
        throw(e); // so that the dialog does not automatically close
        dump ("Exception : RenameFolder \n");
      }
    }
    else 
    {
      dump("no name or nothing selected\n");
    }   
  }
  else 
  {
	  dump("no folder tree\n");
  }
}

function MsgEmptyTrash() 
{
    var folderTree = GetFolderTree();
    var startIndex = {};
    var endIndex = {};
    folderTree.view.selection.getRangeAt(0, startIndex, endIndex);
    if (startIndex.value >= 0)
    {
        var folderResource = GetFolderResource(folderTree, startIndex.value);
        try {
            messenger.EmptyTrash(GetFolderDatasource(), folderResource);
        }
            catch(e)
        {  
            dump ("Exception : messenger.EmptyTrash \n");
        }
    }
}

function MsgCompactFolder(isAll) 
{
  // Get the selected folders.
  var selectedFolders = GetSelectedMsgFolders();

  if (selectedFolders.length == 1)
  {
    var selectedFolder = selectedFolders[0];
    var resource = selectedFolder.QueryInterface(Components.interfaces.nsIRDFResource);

    if (selectedFolder.server.type != "imap") //can be local only
    {
      var msgfolder = resource.QueryInterface(Components.interfaces.nsIMsgFolder);
      var expungedBytes = msgfolder.expungedBytes;

      if (expungedBytes > 0)
      { 
        if (gDBView)
        {
          gCurrentlyDisplayedMessage = gDBView.currentlyDisplayedMessage;
        }

        ClearThreadPaneSelection();
        ClearThreadPane();
        ClearMessagePane();
      }
      else
      {
        if (!isAll)  //you have one local folder with no room to compact
          return;
      }
    } 
    try
    {
      messenger.CompactFolder(GetFolderDatasource(), resource, isAll);
    }
    catch(ex)
    {
      dump("Exception : messenger.CompactFolder : " + ex + "\n");
    }
  }
}

function openNewVirtualFolderDialogWithArgs(defaultViewName, aSearchTerms)
{
  var folderURI = GetSelectedFolderURI();
  var folderTree = GetFolderTree();
  var name = GetFolderNameFromUri(folderURI, folderTree);
  name += "-" + defaultViewName;

  var dialog = window.openDialog("chrome://messenger/content/virtualFolderProperties.xul", "",
                                 "chrome,titlebar,modal,centerscreen",
                                 {preselectedURI:folderURI,
                                  searchTerms:aSearchTerms,
                                  newFolderName:name});
}

function MsgVirtualFolderProperties(aEditExistingVFolder)
{
  var preselectedFolder = GetFirstSelectedMsgFolder();
  var preselectedURI;
  if(preselectedFolder)
  {
    var preselectedFolderResource = preselectedFolder.QueryInterface(Components.interfaces.nsIRDFResource);
    if(preselectedFolderResource)
      preselectedURI = preselectedFolderResource.Value;
  }

  var dialog = window.openDialog("chrome://messenger/content/virtualFolderProperties.xul", "",
                                 "chrome,titlebar,modal,centerscreen",
                                 {preselectedURI:preselectedURI,
                                  editExistingFolder: aEditExistingVFolder,
                                  onOKCallback:onEditVirtualFolderPropertiesCallback,
                                  msgWindow:msgWindow});
}

function onEditVirtualFolderPropertiesCallback(aVirtualFolderURI)
{
  // we need to reload the folder if it is the currently loaded folder...
  if (gMsgFolderSelected && aVirtualFolderURI == gMsgFolderSelected.URI)
  {
    gMsgFolderSelected = null; // force the folder pane to reload the virtual folder
  FolderPaneSelectionChange();
}
}

function MsgFolderProperties() 
{
  var preselectedURI = GetSelectedFolderURI();
  var msgFolder = GetMsgFolderFromUri(preselectedURI, true);

  // if a server is selected, view settings for that account
  if (msgFolder.isServer) {
    MsgAccountManager(null);
    return;
  }

  var serverType = msgFolder.server.type;
  var folderTree = GetFolderTree();

  if (msgFolder.flags & MSG_FOLDER_FLAG_VIRTUAL)
  { 
    // virtual folders get there own property dialog that contains all of the
    // search information related to the virtual folder.
    return MsgVirtualFolderProperties(true);
  }

  var name = GetFolderNameFromUri(preselectedURI, folderTree);

  var windowTitle = gMessengerBundle.getString("folderProperties");
  var dialog = window.openDialog(
              "chrome://messenger/content/folderProps.xul",
              "",
              "chrome,centerscreen,titlebar,modal",
              {preselectedURI:preselectedURI, serverType:serverType,
              msgWindow:msgWindow, title:windowTitle,
              okCallback:FolderProperties, 
              tabID:"", tabIndex:0, name:name});
}

function FolderProperties(name, uri)
{
}

function MsgToggleMessagePane()
{
  var splitter = document.getElementById("threadpane-splitter");
  var state = splitter.getAttribute("state");
  if (state == "collapsed")
    splitter.setAttribute("state", null);
  else
    splitter.setAttribute("state", "collapsed")

   ChangeMessagePaneVisibility(IsMessagePaneCollapsed());
}

function MsgSetFolderCharset() 
{
  MsgFolderProperties() 
}

// Given a URI we would like to return corresponding message folder here.
// An additonal input param which specifies whether or not to check folder 
// attributes (like if there exists a parent or is it a server) is also passed
// to this routine. Qualifying against those checks would return an existing 
// folder. Callers who don't want to check those attributes will specify the
// same and then this routine will simply return a msgfolder. This scenario
// applies to a new imap account creation where special folders are created
// on demand and hence needs to prior check of existence.
function GetMsgFolderFromUri(uri, checkFolderAttributes)
{
    //dump("GetMsgFolderFromUri of " + uri + "\n");
    var msgfolder = null;
    try {
        var resource = GetResourceFromUri(uri);
        msgfolder = resource.QueryInterface(Components.interfaces.nsIMsgFolder);
        if (checkFolderAttributes) {
            if (!(msgfolder && (msgfolder.parent || msgfolder.isServer))) {
                msgfolder = null;
            }
        }
    }
    catch (ex) {
        //dump("failed to get the folder resource\n");
    }
    return msgfolder;
}

function GetResourceFromUri(uri)
{
    var RDF = Components.classes['@mozilla.org/rdf/rdf-service;1'].getService();
    RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);
    var resource = RDF.GetResource(uri);

    return resource;
}  

