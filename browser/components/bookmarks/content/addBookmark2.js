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
 */

var gSelectedFolder = null;
var gName  = null;
var gGroup = null;
var gList  = null;
var gIndentation = null; // temporary hack to indent the folders
const gNameArc = RDF.GetResource(NC_NS+"Name");

function Startup()
{
  gName  = document.getElementById("name");
  gGroup = document.getElementById("addgroup");
  gList  = document.getElementById("select-menu");
  gName.value = window.arguments[0];
  gName.select();
  gName.focus();
  onFieldInput();
  setTimeout(fillSelectFolderMenupopup, 0);
  gIndentation = Array(16);
  gIndentation[0] = "";
  for (var i=1; i<16; ++i)
    gIndentation[i]=gIndentation[i-1]+"  "; 
 
} 

function onFieldInput()
{
  const ok = document.documentElement.getButton("accept");
  ok.disabled = gName.value == "";
}    

function onOK()
{
  document.getElementById("addBookmarkDialog").setAttribute("selectedFolder",gSelectedFolder);
  var rFolder = RDF.GetResource(gSelectedFolder);
  RDFC.Init(BMDS, rFolder);

  var url, rSource;
  if (gGroup && gGroup.checked) {
    rSource = BMDS.createFolder(gName.value);
    const groups  = window.arguments[5];
    for (var i = 0; i < groups.length; ++i) {
      url = getNormalizedURL(groups[i].url);
      BMDS.createBookmarkInContainer(groups[i].name, url,
                                     groups[i].charset, rSource, -1);
    }
  } else {
    url = getNormalizedURL(window.arguments[1]);
    rSource = BMDS.createBookmark(gName.value, url, window.arguments[3]);
  }
  if (!gBMtxmgr)
    gBMtxmgr= BookmarksUtils.getTransactionManager();

  var selection, target;
  selection = BookmarksUtils.getSelectionFromResource(rSource);
  target    = BookmarksUtils.getSelectionFromResource(rFolder);
  target    = BookmarksUtils.getTargetFromSelection(target);
  BookmarksUtils.insertSelection("newbookmark", selection, target);
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

function fillFolder(aPopup, aFolder, aDepth)
{
  RDFC.Init(BMDS, aFolder);
  var children = RDFC.GetElements();
  while (children.hasMoreElements()) {
    var curr = children.getNext();
    if (RDFCU.IsContainer(BMDS, curr)) {
      curr = curr.QueryInterface(Components.interfaces.nsIRDFResource);
      var element = document.createElementNS(XUL_NS, "menuitem");
      var name = BMDS.GetTarget(curr, gNameArc, true).QueryInterface(kRDFLITIID).Value;
      element.setAttribute("label", gIndentation[aDepth]+name);
      element.setAttribute("id", curr.Value);
      aPopup.appendChild(element);
      if (curr.Value == gSelectedFolder)
        gList.selectedItem = element;
      fillFolder(aPopup, curr, ++aDepth);
      --aDepth;
    }
  }
}

function fillSelectFolderMenupopup ()
{

  gSelectedFolder = document.getElementById("addBookmarkDialog").getAttribute("selectedFolder");
  var popup = document.getElementById("select-folder");
  // clearing the old menupopup
  while (popup.hasChildNodes()) 
    popup.removeChild(popup.firstChild);

  // to be removed once I checkin the top folder
  var element = document.createElementNS(XUL_NS, "menuitem");
  element.setAttribute("label", "Bookmarks");
  element.setAttribute("id", "NC:BookmarksRoot");
  popup.appendChild(element);

  var folder = RDF.GetResource("NC:BookmarksRoot");
  fillFolder(popup, folder, 1);
  if (gList.selectedIndex == -1) {
    gList.selectedIndex = 0;
    gSelectedFolder = "NC:BookmarksRoot";
  }
}

function selectFolder(aEvent)
{
  gSelectedFolder = aEvent.target.id;
}
