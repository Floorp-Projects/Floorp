/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <ben@netscape.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 * Add Bookmark Dialog. 
 * ====================
 * 
 * This is a generic bookmark dialog that allows for bookmark addition
 * and folder selection. It can be opened with various parameters that 
 * result in appearance/purpose differences and initial state. 
 * 
 * Use: Open with 'openDialog', with the flags 
 *        'centerscreen,chrome,dialog=no,resizable=yes'
 * 
 * Parameters: 
 *   Apart from the standard openDialog parameters, this dialog can 
 *   be passed additional information, which gets mapped to the 
 *   window.arguments array:
 * 
 *   window.arguments[0]: Bookmark Name. The value to be prefilled
 *                        into the "Name: " field (if visible).
 *   window.arguments[1]: Bookmark URL: The location of the bookmark.
 *                        The value to be filled in the "Location: "
 *                        field (if visible).
 *   window.arguments[2]: Bookmark Folder. The RDF Resource URI of the
 *                        folder that this bookmark should be created in.
 *   window.arguments[3]: Bookmark Charset. The charset that should be
 *                        used when adding a bookmark to the specified
 *                        URL. (Usually the charset of the current 
 *                        document when launching this window). 
 *   window.arguments[4]: The mode of operation. See notes for details.
 *   window.arguments[5]: If the mode is "addGroup", this is an array
 *                        of objects with name, URL and charset
 *                        properties, one for each group member.
 *   window.arguments[6]: If the bookmark should become a web panel.
 *
 * Mode of Operation Notes:
 * ------------------------
 * This dialog can be opened in four different ways by using a parameter
 * passed through the call to openDialog. The 'mode' of operation
 * of the window is expressed in window.arguments[4]. The valid modes are:
 *
 * 1) <default> (no fifth open parameter).
 *      Opens this dialog with the bookmark Name, URL and folder selection
 *      components visible. 
 * 2) "newBookmark" (fifth open parameter = String("newBookmark"))
 *      Opens the dialog as in (1) above except the folder selection tree
 *      is hidden. This type of mode is useful when the creation folder 
 *      is pre-determined.
 * 3) "selectFolder" (fifth open parameter = String("selectFolder"))
 *      Opens the dialog as in (1) above except the Name/Location section
 *      is hidden, and the dialog takes on the utility of a Folder chooser.
 *      Used when the user must select a Folder for some purpose. 
 * 4) "addGroup" (fifth open parameter = String("addGroup"))
 *      Opens the dialog like <default>, with a checkbox to select between
 *      filing a single bookmark or a group. For the single bookmark the
 *      values are taken from the name, URL and charset arguments.
 *      For the group, the values are taken from the sixth argument.
 *      This parameter can also be String("addGroup,group") where "group"
 *      specifies that the dialog starts in filing as a group.
 */

var gFld_Name   = null;
var gFld_URL    = null; 
var gFolderTree = null;
var gCB_AddGroup = null;

var gBookmarkCharset = null;

var gSelectItemObserver = null;

var gCreateInFolder = "NC:NewBookmarkFolder";

function Startup()
{
  initServices();
  initBMService();
  gFld_Name = document.getElementById("name");
  gFld_URL = document.getElementById("url");
  gCB_AddGroup = document.getElementById("addgroup");
  var bookmarkView = document.getElementById("bookmarks-view");

  var shouldSetOKButton = true;
  var dialogElement = document.documentElement;
  if ("arguments" in window) {
    var ind;
    var folderItem = null;
    var arg;
    if (window.arguments.length < 5)
      arg = null;
    else
      arg = window.arguments[4];
    switch (arg) {
    case "selectFolder":
      // If we're being opened as a folder selection window
      document.getElementById("bookmarknamegrid").setAttribute("hidden", "true");
      document.getElementById("createinseparator").setAttribute("hidden", "true");
      document.getElementById("nameseparator").setAttribute("hidden", "true");
      sizeToContent();
      dialogElement.setAttribute("title", dialogElement.getAttribute("title-selectFolder"));
      shouldSetOKButton = false;
      if (window.arguments[2])
        folderItem = RDF.GetResource(window.arguments[2]);
      if (folderItem) {
        ind = bookmarkView.treeBuilder.getIndexOfResource(folderItem);
        bookmarkView.treeBoxObject.view.selection.select(ind);
      }
      break;
    case "newBookmark":
      setupFields();
      if (window.arguments[2])
        gCreateInFolder = window.arguments[2];
      document.getElementById("folderbox").setAttribute("hidden", "true");
      sizeToFit();
      break;
    case "addGroup":
      document.getElementById("showaddgroup").setAttribute("hidden", "false");
      setupFields();
      sizeToFit();
      break;
    case "addGroup,group":
      document.getElementById("showaddgroup").setAttribute("hidden", "false");
      gCB_AddGroup.setAttribute("checked", "true");
      setupFields();
      toggleGroup();
      sizeToFit();
      break;
    default:
      // Regular Add Bookmark
      setupFields();
      if (window.arguments[2]) {
        gCreateInFolder = window.arguments[2];
        folderItem = bookmarkView.rdf.GetResource(gCreateInFolder);
        if (folderItem) {
          ind = bookmarkView.treeBuilder.getIndexOfResource(folderItem);
          bookmarkView.treeBoxObject.view.selection.select(ind);
        }
      }
    }
  }
  
  if (shouldSetOKButton)
    onFieldInput();
  if (document.getElementById("bookmarknamegrid").hasAttribute("hidden")) {
    bookmarkView.tree.focus();
    if (bookmarkView.currentIndex == -1)
      bookmarkView.treeBoxObject.view.selection.select(0);
  }
  else {
    gFld_Name.select();
    gFld_Name.focus();
  }
} 

function sizeToFit()
{
  var dialogElement = document.documentElement;
  dialogElement.removeAttribute("persist");
  dialogElement.removeAttribute("height");
  dialogElement.removeAttribute("width");
  dialogElement.setAttribute("style", dialogElement.getAttribute("style"));
  sizeToContent();
}

function setupFields()
{
  // New bookmark in predetermined folder. 
  gFld_Name.value = window.arguments[0] || "";
  gFld_URL.value = window.arguments[1] || "";
  onFieldInput();
  gFld_Name.select();
  gFld_Name.focus();
  gBookmarkCharset = window.arguments[3] || null;
}

function onFieldInput()
{
  const ok = document.documentElement.getButton("accept");
  ok.disabled = gFld_URL.value == "" && !addingGroup() ||
                gFld_Name.value == "";
}    

function onOK()
{
  if (!document.getElementById("folderbox").hasAttribute("hidden")) {
    var bookmarkView = document.getElementById("bookmarks-view");
    var currentIndex = bookmarkView.currentIndex;
    if (currentIndex != -1)
      gCreateInFolder = bookmarkView.treeBuilder.getResourceAtIndex(currentIndex).Value;
  }
  // In Select Folder Mode, do nothing but tell our caller what
  // folder was selected. 
  if (window.arguments.length > 4 && window.arguments[4] == "selectFolder")
    window.arguments[5].target = BookmarksUtils.getTargetFromFolder(bookmarkView.treeBuilder.getResourceAtIndex(currentIndex));
  else {
    // Otherwise add a bookmark to the selected folder. 
    var rFolder = RDF.GetResource(gCreateInFolder);
    try {
      RDFC.Init(BMDS, rFolder);
    }
    catch (e) {
      // No "NC:NewBookmarkFolder" exists, just append to the root.
      rFolder = RDF.GetResource("NC:BookmarksRoot");
      RDFC.Init(BMDS, rFolder);
    }

    // if no URL was provided and we're not filing as a group, do nothing
    if (!gFld_URL.value && !addingGroup())
      return;

    var url, rSource;
    if (addingGroup()) {
      rSource = BMDS.createFolder(gFld_Name.value);
      const groups  = window.arguments[5];
      for (var i = 0; i < groups.length; ++i) {
        url = getNormalizedURL(groups[i].url);
        BMDS.createBookmarkInContainer(groups[i].name, url, null, null,
                                       groups[i].charset, null, rSource, -1);
      }
    } else {
      url = getNormalizedURL(gFld_URL.value);
      rSource = BMDS.createBookmark(gFld_Name.value, url, null, null, gBookmarkCharset, false, "");
      if (window.arguments.length > 4 && window.arguments[4] == "newBookmark") {
        window.arguments[5].newBookmark = rSource;
      }
    }
    var selection = BookmarksUtils.getSelectionFromResource(rSource);
    var target    = BookmarksUtils.getTargetFromFolder(rFolder);
    BookmarksUtils.insertAndCheckSelection("newbookmark", selection, target);
  }
}

function getNormalizedURL(url)
{
  // Check to see if the item is a local directory path, and if so, convert
  // to a file URL so that aggregation with rdf:files works
  try {
    const kLF = Components.classes["@mozilla.org/file/local;1"]
                          .createInstance(Components.interfaces.nsILocalFile);
    kLF.initWithPath(url);
    if (kLF.exists()) {
      var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                                .getService(Components.interfaces.nsIIOService);
      var fileHandler = ioService.getProtocolHandler("file")
                                 .QueryInterface(Components.interfaces.nsIFileProtocolHandler);

      url = fileHandler.getURLSpecFromFile(kLF);
    }
  }
  catch (e) {
  }

  return url;
}

function createNewFolder ()
{
  var bookmarksView = document.getElementById("bookmarks-view");
  var resource = bookmarksView.treeBuilder.getResourceAtIndex(bookmarksView.currentIndex);
  var target = BookmarksUtils.getTargetFromFolder(resource);
  BookmarksCommand.createNewFolder(target);
}

function useDefaultFolder ()
{
  var bookmarkView = document.getElementById("bookmarks-view");
  var folder = BookmarksUtils.getNewBookmarkFolder();
  var ind = bookmarkView.treeBuilder.getIndexOfResource(folder);
  if (ind != -1) {
    bookmarkView.tree.focus();
    bookmarkView.treeBoxObject.view.selection.select(ind);
  } else {
    bookmarkView.treeBoxObject.view.selection.clearSelection();
  }
  gCreateInFolder = folder.Value;
}

var gOldNameValue = "";
var gOldURLValue = "";

function toggleGroup()
{
  // swap between single bookmark and group name
  var temp = gOldNameValue;
  gOldNameValue = gFld_Name.value;
  gFld_Name.value = temp;

  // swap between single bookmark and group url
  temp = gOldURLValue;
  gOldURLValue = gFld_URL.value;
  gFld_URL.value = temp;
  gFld_URL.disabled = gCB_AddGroup.getAttribute("checked") == "true";

  gFld_Name.select();
  gFld_Name.focus();
  onFieldInput();
}

function addingGroup()
{
  const showAddGroup = document.getElementById("showaddgroup");
  return showAddGroup.getAttribute("hidden") != "true" && gCB_AddGroup.getAttribute("checked") == "true";
}
