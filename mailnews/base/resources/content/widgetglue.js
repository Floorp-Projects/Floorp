/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jan Varga (varga@nixcorp.com)
 *   Håkan Waara (hwaara@chello.se)
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

  // we'll probably want a new "server type" for virtual folders
  // that will allow the user to edit the search criteria.
  // But for now, we'll just disable other properties.
  if (msgFolder.flags & MSG_FOLDER_FLAG_VIRTUAL)
    serverType = "none";

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

function FolderProperties(name, oldName, uri)
{
  if (name != oldName)
    RenameFolder(name, uri);
}

function MsgToggleMessagePane()
{
  //OnClickThreadAndMessagePaneSplitter is based on the value before the splitter is toggled.
  OnClickThreadAndMessagePaneSplitterGrippy();
  MsgToggleSplitter("threadpane-splitter");
}

function MsgToggleSplitter(id)
{
    var splitter = document.getElementById(id);
    var state = splitter.getAttribute("state");
    if (state == "collapsed")
        splitter.setAttribute("state", null);
    else
        splitter.setAttribute("state", "collapsed")
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

