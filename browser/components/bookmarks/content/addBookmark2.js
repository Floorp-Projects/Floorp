# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
# ***** BEGIN LICENSE BLOCK *****
# Version: NPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Netscape Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is mozilla.org code.
# 
# The Initial Developer of the Original Code is 
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#   Ben Goodger <ben@netscape.com> (Original Author)
#   David Hyatt <hyatt@mozilla.org>
# 
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or 
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the NPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the NPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK ***** */

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
 *   XXXben - it would be cleaner just to take this dialog take an
 *            object so items don't need to be passed in a particular
 *            order.
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
 *   window.arguments[7]: A suggested keyword for the bookmark. If this
 *                        argument is supplied, the keyword row is made
 *                        visible.
 *   window.arguments[8]: Whether or not a keyword is required to add
 *                        the bookmark.
 *   window.arguments[9]: PostData to be saved with this bookmark, 
 *                        in the format a string of name=value pairs
 *                        separated by CRLFs.
 *   window.arguments[10]: feed URL for Livemarks (turns bookmark
 *                         into Livemark)
 */

var gSelectedFolder;
var gName;
var gKeyword;
var gKeywordRow;
var gExpander;
var gMenulist;
var gBookmarksTree;
var gGroup;
var gKeywordRequired;
var gSuggestedKeyword;
var gRequiredFields = [];
var gPostData;

# on windows, sizeToContent is buggy (see bug 227951), we''ll use resizeTo
# instead and cache the bookmarks tree view size.
var WSucks;

function Startup()
{
  initServices();
  initBMService();
  gName = document.getElementById("name");
  gRequiredFields.push(gName);
  gKeywordRow = document.getElementById("keywordRow");
  gKeyword = document.getElementById("keyword");
  gGroup = document.getElementById("addgroup");
  gExpander = document.getElementById("expander");
  gMenulist = document.getElementById("select-menu");
  gBookmarksTree = document.getElementById("folder-tree");
  gName.value = window.arguments[0];
  gName.select();
  gName.focus();
  gSuggestedKeyword = window.arguments[7];
  gKeywordRequired = window.arguments[8];
  if (!gSuggestedKeyword && !gKeywordRequired) {
    gKeywordRow.hidden = true;
  } else {
    if (gSuggestedKeyword)
      gKeyword.value = gSuggestedKeyword;
    if (gKeywordRequired)
      gRequiredFields.push(gKeyword);
  }
  sizeToContent(); // do this here to ensure we size properly without the keyword field
  onFieldInput();
  gSelectedFolder = RDF.GetResource(gMenulist.selectedItem.id);
  gExpander.setAttribute("tooltiptext", gExpander.getAttribute("tooltiptextdown"));
  gPostData = window.arguments[9];

# read the persisted attribute. If it is not present, set a default height.
  WSucks = parseInt(gBookmarksTree.getAttribute("height"));
  if (!WSucks)
    WSucks = 150;

  // fix no more persisted class attribute in old profiles
  var localStore = RDF.GetDataSource("rdf:local-store");
  var rAttribute = RDF.GetResource("class");
  var rElement   = RDF.GetResource("chrome://browser/content/bookmarks/addBookmark2.xul#expander");
  var rDialog    = RDF.GetResource("chrome://browser/content/bookmarks/addBookmark2.xul");
  var rPersist   = RDF.GetResource(NC_NS+"persist");
  
  var rOldValue = localStore.GetTarget(rElement, rAttribute, true);
  if (rOldValue) {
    localStore.Unassert(rElement, rAttribute, rOldValue, true);
    localStore.Unassert(rDialog, rPersist, rElement, true);
    gExpander.setAttribute("class", "down");
  }
} 

function onFieldInput()
{
  var ok = document.documentElement.getButton("accept");
  ok.disabled = false;
  for (var i = 0; i < gRequiredFields.length; ++i) {
    if (gRequiredFields[i].value == "") {
      ok.disabled = true;
      return;
    }
  }
}

function onOK()
{
  RDFC.Init(BMDS, gSelectedFolder);

  var url, rSource;
  var livemarkFeed = window.arguments[10];
  if (gGroup && gGroup.checked) {
    rSource = BMDS.createFolder(gName.value);
    const groups = window.arguments[5];
    for (var i = 0; i < groups.length; ++i) {
      url = getNormalizedURL(groups[i].url);
      BMDS.createBookmarkInContainer(groups[i].name, url, gKeyword.value, null,
                                     groups[i].charset, gPostData, rSource, -1);
    }
  } else if (livemarkFeed != null) {
    url = getNormalizedURL(window.arguments[1]);
    rSource = BMDS.createLivemark(gName.value, url, livemarkFeed, null);
  } else {
    url = getNormalizedURL(window.arguments[1]);
    rSource = BMDS.createBookmark(gName.value, url, gKeyword.value, null, window.arguments[3], gPostData);
  }

  var selection = BookmarksUtils.getSelectionFromResource(rSource);
  var target    = BookmarksUtils.getTargetFromFolder(gSelectedFolder);
  BookmarksUtils.insertAndCheckSelection("newbookmark", selection, target);
  
  if (window.arguments[6] && rSource) {
    // Assert that we're a web panel.
    BMDS.Assert(rSource, RDF.GetResource(NC_NS+"WebPanel"),
                RDF.GetLiteral("true"), true);
  }
  
  // in insertSelection, the ds flush is delayed. It will never be performed,
  // since this dialog is destroyed before.
  // We have to flush manually
  var remoteDS = BMDS.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
  remoteDS.Flush();
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

function selectMenulistFolder(aEvent)
{
  gSelectedFolder = RDF.GetResource(aEvent.target.id);
  if (!gBookmarksTree.collapsed)
    selectFolder(gSelectedFolder);
}

function selectTreeFolder()
{
  var resource = gBookmarksTree.currentResource;
  if (resource == gSelectedFolder)
    return;
  gSelectedFolder = resource;
  var menuitem = document.getElementById(gSelectedFolder.Value);
  gMenulist.selectedItem = menuitem;
  if (!menuitem)
    gMenulist.label = BookmarksUtils.getProperty(gSelectedFolder, NC_NS+"Name");
}

function selectFolder(aFolder)
{
  gBookmarksTree.treeBoxObject.view.selection.selectEventsSuppressed = true;
  gBookmarksTree.treeBoxObject.view.selection.clearSelection();
  gBookmarksTree.selectResource(aFolder);
  var index = gBookmarksTree.currentIndex;
  gBookmarksTree.treeBoxObject.ensureRowIsVisible(index);
  gBookmarksTree.treeBoxObject.view.selection.selectEventsSuppressed = false;
# triggers a select event that will provoke a call to selectTreeFolder()
}

function expandTree()
{
  setFolderTreeHeight();
  var willCollapse = !gBookmarksTree.collapsed;
  gExpander.setAttribute("class",willCollapse?"down":"up");
  gExpander.setAttribute("tooltiptext", gExpander.getAttribute("tooltiptext"+(willCollapse?"down":"up")));
  if (willCollapse) {
    document.documentElement.buttons = "accept,cancel";
    WSucks = gBookmarksTree.boxObject.height;
  } else {
    document.documentElement.buttons = "accept,cancel,extra2";
#   always open the bookmark root folder
    if (!gBookmarksTree.treeBoxObject.view.isContainerOpen(0))
      gBookmarksTree.treeBoxObject.view.toggleOpenState(0);
    selectFolder(gSelectedFolder);
    gBookmarksTree.focus();
  }
  gBookmarksTree.collapsed = willCollapse;
  resizeTo(window.outerWidth, window.outerHeight+(willCollapse?-WSucks:+WSucks));
}

function setFolderTreeHeight()
{
  var isCollapsed = gBookmarksTree.collapsed;
  if (!isCollapsed)
    gBookmarksTree.setAttribute("height", gBookmarksTree.boxObject.height);
}

function newFolder()
{
  gBookmarksTree.focus();
  // we should use goDoCommand, but the current way of inserting
  // resources do not insert in folders.
  //goDoCommand("cmd_bm_newfolder");
  var target = BookmarksUtils.getTargetFromFolder(gSelectedFolder);
  var folder = BookmarksCommand.createNewFolder(target);
  if (!BMSVC.isBookmarkedResource(folder))
    return; // new folder cancelled
  selectFolder(folder);
}
