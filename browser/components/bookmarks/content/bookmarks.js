# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
# ***** END LICENSE BLOCK *****

var NC_NS, WEB_NS, RDF_NS, XUL_NS, NC_NS_CMD;

// definition of the services frequently used for bookmarks
var kRDFContractID;
var kRDFSVCIID;
var kRDFRSCIID;
var kRDFLITIID;
var RDF;

var kRDFCContractID;
var kRDFCIID;
var RDFC;

var kRDFCUContractID;
var kRDFCUIID;
var RDFCU;

var BMDS;
var kBMSVCIID;
var BMSVC;

var kPREFContractID;
var kPREFIID;
var PREF;

var kSOUNDContractID;
var kSOUNDIID;
var SOUND;

var kWINDOWContractID;
var kWINDOWIID;
var WINDOWSVC;

var kDSContractID;
var kDSIID;
var DS;

// should be moved in a separate file
function initServices()
{
  NC_NS     = "http://home.netscape.com/NC-rdf#";
  WEB_NS    = "http://home.netscape.com/WEB-rdf#";
  RDF_NS    = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
  XUL_NS    = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
  NC_NS_CMD = NC_NS + "command?cmd=";

  kRDFContractID   = "@mozilla.org/rdf/rdf-service;1";
  kRDFSVCIID       = Components.interfaces.nsIRDFService;
  kRDFRSCIID       = Components.interfaces.nsIRDFResource;
  kRDFLITIID       = Components.interfaces.nsIRDFLiteral;
  RDF              = Components.classes[kRDFContractID].getService(kRDFSVCIID);

  kRDFCContractID  = "@mozilla.org/rdf/container;1";
  kRDFCIID         = Components.interfaces.nsIRDFContainer;
  RDFC             = Components.classes[kRDFCContractID].createInstance(kRDFCIID);

  kRDFCUContractID = "@mozilla.org/rdf/container-utils;1";
  kRDFCUIID        = Components.interfaces.nsIRDFContainerUtils;
  RDFCU            = Components.classes[kRDFCUContractID].getService(kRDFCUIID);

  kPREFContractID  = "@mozilla.org/preferences-service;1";
  kPREFIID         = Components.interfaces.nsIPrefService;
  PREF             = Components.classes[kPREFContractID].getService(kPREFIID)
                               .getBranch(null);

  kSOUNDContractID = "@mozilla.org/sound;1";
  kSOUNDIID        = Components.interfaces.nsISound;
  SOUND            = Components.classes[kSOUNDContractID].createInstance(kSOUNDIID);

  kWINDOWContractID = "@mozilla.org/appshell/window-mediator;1";
  kWINDOWIID        = Components.interfaces.nsIWindowMediator;
  WINDOWSVC         = Components.classes[kWINDOWContractID].getService(kWINDOWIID);

  kDSContractID     = "@mozilla.org/widget/dragservice;1";
  kDSIID            = Components.interfaces.nsIDragService;
  DS                = Components.classes[kDSContractID].getService(kDSIID);

}

function initBMService()
{
  kBMSVCIID = Components.interfaces.nsIBookmarksService;
  BMDS  = RDF.GetDataSource("rdf:bookmarks");
  BMSVC = BMDS.QueryInterface(kBMSVCIID);
  BookmarkTransaction.prototype.RDFC = RDFC;
  BookmarkTransaction.prototype.BMDS = BMDS;
}

/**
 * XXX - 04/16/01
 *  ACK! massive command name collision problems are causing big issues
 *  in getting this stuff to work in the Navigator window. For sanity's 
 *  sake, we need to rename all the commands to be of the form cmd_bm_*
 *  otherwise there'll continue to be problems. For now, we're just 
 *  renaming those that affect the personal toolbar (edit operations,
 *  which were clashing with the textfield controller)
 *
 * There are also several places that need to be updated if you need
 * to change a command name. 
 *   1) the controller...
 *      - in bookmarksTree.xml if the command is tree-specifc
 *      - in bookmarksMenu.js if the command is DOM-specific
 *      - in bookmarks.js otherwise
 *   2) the command nodes in the overlay or xul file
 *   3) the command human-readable name key in bookmarks.properties
 *   4) the function 'getCommands' in bookmarks.js
 */

var BookmarksCommand = {

  /////////////////////////////////////////////////////////////////////////////
  // This method constructs a menuitem for a context menu for the given command.
  // This is implemented by the client so that it can intercept menuitem naming
  // as appropriate.
  createMenuItem: function (aDisplayName, aCommandName, aSelection)
  {
    var xulElement = document.createElementNS(XUL_NS, "menuitem");
    xulElement.setAttribute("cmd", aCommandName);
    var cmd = "cmd_" + aCommandName.substring(NC_NS_CMD.length);
    xulElement.setAttribute("command", cmd);
    
    switch (aCommandName) {
    case NC_NS_CMD + "bm_expandfolder":
      var shouldCollapse = true;
      for (var i=0; i<aSelection.length; ++i)
        if (!aSelection.isExpanded[i])
          shouldCollapse = false;
      aDisplayName =  shouldCollapse? BookmarksUtils.getLocaleString("cmd_bm_collapsefolder") : aDisplayName;
    break;
    }
    xulElement.setAttribute("label", aDisplayName);
    return xulElement;
  },

  /////////////////////////////////////////////////////////////////////////////
  // Fill a context menu popup with menuitems that are appropriate for the current
  // selection.
  createContextMenu: function (aEvent, aSelection)
  {
    var popup = aEvent.target;
    // clear out the old context menu contents (if any)
    while (popup.hasChildNodes()) 
      popup.removeChild(popup.firstChild);
        
    var commonCommands = [];
    for (var i = 0; i < aSelection.length; ++i) {
      var commands = this.getCommands(aSelection.item[i]);
      if (!commands) {
        aEvent.preventDefault();
        return;
      }
      commands = this.flattenEnumerator(commands);
      if (!commonCommands.length) commonCommands = commands;
      commonCommands = this.findCommonNodes(commands, commonCommands);
    }

    if (!commonCommands.length) {
      aEvent.preventDefault();
      return;
    }
    
    // Now that we should have generated a list of commands that is valid
    // for the entire selection, build a context menu.
    for (i = 0; i < commonCommands.length; ++i) {
      var currCommand = commonCommands[i].QueryInterface(kRDFRSCIID).Value;
      var element = null;
      if (currCommand != NC_NS_CMD + "bm_separator") {
        var commandName = this.getCommandName(currCommand);
        element = this.createMenuItem(commandName, currCommand, aSelection);
      }
      else if (i != 0 && i < commonCommands.length-1) {
        // Never append a separator as the first or last element in a context
        // menu.
        element = document.createElementNS(XUL_NS, "menuseparator");
      }
      if (element) 
        popup.appendChild(element);
    }
    switch (popup.firstChild.getAttribute("command")) {
    case "cmd_bm_open":
    case "cmd_bm_expandfolder":
      popup.firstChild.setAttribute("default", "true");
    }
  },
  
  /////////////////////////////////////////////////////////////////////////////
  // Given two unique arrays, return an array that contains only the elements
  // common to both. 
  findCommonNodes: function (aNewArray, aOldArray)
  {
    var common = [];
    for (var i = 0; i < aNewArray.length; ++i) {
      for (var j = 0; j < aOldArray.length; ++j) {
        if (common.length > 0 && common[common.length-1] == aNewArray[i])
          continue;
        if (aNewArray[i] == aOldArray[j])
          common.push(aNewArray[i]);
      }
    }
    return common;
  },

  flattenEnumerator: function (aEnumerator)
  {
    if ("_index" in aEnumerator)
      return aEnumerator._inner;
    
    var temp = [];
    while (aEnumerator.hasMoreElements()) 
      temp.push(aEnumerator.getNext());
    return temp;
  },
  
  /////////////////////////////////////////////////////////////////////////////
  // For a given URI (a unique identifier of a resource in the graph) return 
  // an enumeration of applicable commands for that URI. 
  getCommands: function (aNodeID)
  {
    var type = BookmarksUtils.resolveType(aNodeID);
    if (!type)
      return null;
    var commands = [];
    // menu order:
    // 
    // bm_expandfolder
    // bm_open, bm_openfolder
    // bm_openinnewwindow
    // bm_openinnewtab
    // ---------------------
    // bm_newfolder
    // ---------------------
    // cut
    // copy
    // paste
    // ---------------------
    // delete
    // ---------------------
    // bm_properties
    switch (type) {
    case "BookmarkSeparator":
      commands = ["bm_newfolder", "bm_separator", 
                  "cut", "copy", "paste", "bm_separator",
                  "delete", "bm_separator",
                  "bm_properties"];
      break;
    case "Bookmark":
      commands = ["bm_open", "bm_openinnewwindow", "bm_openinnewtab", "bm_separator",
                  "bm_newfolder", "bm_separator",
                  "cut", "copy", "paste", "bm_separator",
                  "delete", "bm_separator",
                  "bm_properties"];
      break;
    case "Folder":
      commands = ["bm_expandfolder", "bm_openfolder", "bm_managefolder", "bm_separator", 
                  "bm_newfolder", "bm_separator",
                  "cut", "copy", "paste", "bm_separator",
                  "delete", "bm_separator",
                  "bm_properties"];
      break;
    case "PersonalToolbarFolder":
      commands = ["bm_newfolder", "bm_separator",
                  "cut", "copy", "paste", "bm_separator",
                  "delete", "bm_separator",
                  "bm_properties"];
      break;
    case "IEFavoriteFolder":
      commands = ["bm_expandfolder", "bm_separator", "delete"];
      break;
    case "IEFavorite":
      commands = ["bm_open", "bm_openinnewwindow", "bm_openinnewtab", "bm_separator",
                  "copy"];
      break;
    case "FileSystemObject":
      commands = ["bm_open", "bm_openinnewwindow", "bm_openinnewtab", "bm_separator",
                  "copy"];
      break;
    default: 
      commands = [];
    }

    return new CommandArrayEnumerator(commands);
  },
  
  /////////////////////////////////////////////////////////////////////////////
  // Retrieve the human-readable name for a particular command. Used when 
  // manufacturing a UI to invoke commands.
  getCommandName: function (aCommand) 
  {
    var cmdName = aCommand.substring(NC_NS_CMD.length);
    return BookmarksUtils.getLocaleString ("cmd_" + cmdName);
    /*
    try {
      // Note: this will succeed only if there's a string in the bookmarks
      //       string bundle for this command name. Otherwise, <xul:stringbundle/>
      //       will throw, we'll catch & stifle the error, and look up the command
      //       name in the datasource. 
      return BookmarksUtils.getLocaleString ("cmd_" + cmdName);
    }
    catch (e) {
    }   
    // XXX - WORK TO DO HERE! (rjc will cry if we don't fix this) 
    // need to ask the ds for the commands for this node, however we don't
    // have the right params. This is kind of a problem. 
    dump("*** BAD! EVIL! WICKED! NO! ACK! ARGH! ORGH!"+aCommand+"\n");
    const rName = RDF.GetResource(NC_NS + "Name");
    const rSource = RDF.GetResource(aNodeID);
    return BMDS.GetTarget(rSource, rName, true).Value;
    */
  },
    
  ///////////////////////////////////////////////////////////////////////////
  // Execute a command with the given source and arguments
  doBookmarksCommand: function (aSource, aCommand, aArgumentsArray)
  {
    var rCommand = RDF.GetResource(aCommand);
  
    var kSuppArrayContractID = "@mozilla.org/supports-array;1";
    var kSuppArrayIID = Components.interfaces.nsISupportsArray;
    var sourcesArray = Components.classes[kSuppArrayContractID].createInstance(kSuppArrayIID);
    if (aSource) {
      sourcesArray.AppendElement(aSource);
    }
  
    var argsArray = Components.classes[kSuppArrayContractID].createInstance(kSuppArrayIID);
    var length = aArgumentsArray?aArgumentsArray.length:0;
    for (var i = 0; i < length; ++i) {
      var rArc = RDF.GetResource(aArgumentsArray[i].property);
      argsArray.AppendElement(rArc);
      var rValue = null;
      if ("resource" in aArgumentsArray[i]) { 
        rValue = RDF.GetResource(aArgumentsArray[i].resource);
      }
      else
        rValue = RDF.GetLiteral(aArgumentsArray[i].literal);
      argsArray.AppendElement(rValue);
    }

    // Exec the command in the Bookmarks datasource. 
    BMDS.DoCommand(sourcesArray, rCommand, argsArray);
  },

  undoBookmarkTransaction: function ()
  {
    BMSVC.transactionManager.undoTransaction();
    BookmarksUtils.flushDataSource();
  },

  redoBookmarkTransaction: function ()
  {
    BMSVC.transactionManager.redoTransaction();
    BookmarksUtils.flushDataSource();
  },

  manageFolder: function (aSelection)
  {
    openDialog("chrome://browser/content/bookmarks/bookmarksManager.xul", 
               "", "chrome,all,dialog=no", aSelection.item[0].Value);
  },
  
  cutBookmark: function (aSelection)
  {
    this.copyBookmark(aSelection);
    BookmarksUtils.removeAndCheckSelection("cut", aSelection);
  },

  copyBookmark: function (aSelection)
  {

    const kSuppArrayContractID = "@mozilla.org/supports-array;1";
    const kSuppArrayIID = Components.interfaces.nsISupportsArray;
    var itemArray = Components.classes[kSuppArrayContractID].createInstance(kSuppArrayIID);

    const kSuppWStringContractID = "@mozilla.org/supports-string;1";
    const kSuppWStringIID = Components.interfaces.nsISupportsString;
    var bmstring = Components.classes[kSuppWStringContractID].createInstance(kSuppWStringIID);
    var unicodestring = Components.classes[kSuppWStringContractID].createInstance(kSuppWStringIID);
    var htmlstring = Components.classes[kSuppWStringContractID].createInstance(kSuppWStringIID);
  
    var sBookmarkItem = ""; var sTextUnicode = ""; var sTextHTML = "";
    for (var i = 0; i < aSelection.length; ++i) {
      var url  = BookmarksUtils.getProperty(aSelection.item[i], NC_NS+"URL" );
      var name = BookmarksUtils.getProperty(aSelection.item[i], NC_NS+"Name");
      sBookmarkItem += aSelection.item[i].Value + "\n";
      sTextUnicode += url + "\n";
      sTextHTML += "<A HREF=\"" + url + "\">" + name + "</A>";
    }
    
    const kXferableContractID = "@mozilla.org/widget/transferable;1";
    const kXferableIID = Components.interfaces.nsITransferable;
    var xferable = Components.classes[kXferableContractID].createInstance(kXferableIID);

    xferable.addDataFlavor("moz/bookmarkclipboarditem");
    bmstring.data = sBookmarkItem;
    xferable.setTransferData("moz/bookmarkclipboarditem", bmstring, sBookmarkItem.length*2);
    
    xferable.addDataFlavor("text/html");
    htmlstring.data = sTextHTML;
    xferable.setTransferData("text/html", htmlstring, sTextHTML.length*2);
    
    xferable.addDataFlavor("text/unicode");
    unicodestring.data = sTextUnicode;
    xferable.setTransferData("text/unicode", unicodestring, sTextUnicode.length*2);
    
    const kClipboardContractID = "@mozilla.org/widget/clipboard;1";
    const kClipboardIID = Components.interfaces.nsIClipboard;
    var clipboard = Components.classes[kClipboardContractID].getService(kClipboardIID);
    clipboard.setData(xferable, null, kClipboardIID.kGlobalClipboard);
  },

  pasteBookmark: function (aTarget)
  {
    const kXferableContractID = "@mozilla.org/widget/transferable;1";
    const kXferableIID = Components.interfaces.nsITransferable;
    var xferable = Components.classes[kXferableContractID].createInstance(kXferableIID);
    xferable.addDataFlavor("moz/bookmarkclipboarditem");
    xferable.addDataFlavor("text/x-moz-url");
    xferable.addDataFlavor("text/unicode");

    const kClipboardContractID = "@mozilla.org/widget/clipboard;1";
    const kClipboardIID = Components.interfaces.nsIClipboard;
    var clipboard = Components.classes[kClipboardContractID].getService(kClipboardIID);
    clipboard.getData(xferable, kClipboardIID.kGlobalClipboard);
    
    var flavour = { };
    var data    = { };
    var length  = { };
    xferable.getAnyTransferData(flavour, data, length);
    var items, name, url;
    data = data.value.QueryInterface(Components.interfaces.nsISupportsString).data;
    switch (flavour.value) {
    case "moz/bookmarkclipboarditem":
      items = data.split("\n");
      // since data are ended by \n, remove the last empty node
      items.pop(); 
      for (var i=0; i<items.length; ++i) {
        items[i] = RDF.GetResource(items[i]);
      }
      break;
    case "text/x-moz-url":
      // there should be only one item in this case
      var ix = data.indexOf("\n");
      items = data.substring(0, ix != -1 ? ix : data.length);
      name  = data.substring(ix);
      // XXX: we should infer the best charset
      BookmarksUtils.createBookmark(null, items, null, name, null);
      items = [items];
      break;
    default: 
      return;
    }
   
    var selection = {item: items, parent:Array(items.length), length: items.length};
    BookmarksUtils.checkSelection(selection);
    BookmarksUtils.insertAndCheckSelection("paste", selection, aTarget);
  },
  
  deleteBookmark: function (aSelection)
  {
    BookmarksUtils.removeAndCheckSelection("delete", aSelection);
  },

  moveBookmark: function (aSelection)
  {
    var rv = { selectedFolder: null };      
    openDialog("chrome://browser/content/bookmarks/addBookmark.xul", "", 
               "centerscreen,chrome,modal=yes,dialog=yes,resizable=yes", null, null, null, null, "selectFolder", rv);
    if (!rv.target)
      return;
    
    var target = rv.target;
    BookmarksUtils.moveSelection("move", aSelection, target);
  },

  openBookmark: function (aSelection, aTargetBrowser, aDS) 
  {
    if (!aTargetBrowser)
      return;
    for (var i=0; i<aSelection.length; ++i) {
      var type = aSelection.type[i];
      if (aTargetBrowser == "save") {
        var item = aSelection.item[i];
        saveURL(item.Value, BookmarksUtils.getProperty(item, "Name"), null, true);
      }
      else if (type == "Bookmark" || type == "") {
        var webPanel = BMDS.GetTarget(aSelection.item[i],
                                      RDF.GetResource(NC_NS + "WebPanel"),
                                      true);
        if (webPanel && aTargetBrowser == "current")
          this.openWebPanel(aSelection.item[i].Value, aDS);
        else
          this.openOneBookmark(aSelection.item[i].Value, aTargetBrowser, aDS);
      }
      else if (type == "Folder" || type == "PersonalToolbarFolder")
        this.openGroupBookmark(aSelection.item[i].Value, aTargetBrowser);
    }
  },
  
  openBookmarkProperties: function (aSelection) 
  {
    // Bookmark Properties dialog is only ever opened with one selection 
    // (command is disabled otherwise)
    var bookmark = aSelection.item[0].Value;
    value = {};
    openDialog("chrome://browser/content/bookmarks/bookmarksProperties.xul", "", "centerscreen,chrome,modal,resizable=no", bookmark, value);
    return value.ok;
  },

  // requires utilityOverlay.js if opening in new window for getTopWin()
  openWebPanel: function(aResource, aDS)
  {
    var url = BookmarksUtils.getProperty(aResource, NC_NS+"URL", aDS);
    // Ignore "NC:" and empty urls.
    if (url == "")
      return;
    var w = getTopWin();
    if (!w) {
      openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no", url);
      return;
    }
    w.openWebPanel(BookmarksUtils.getProperty(aResource,  NC_NS+"Name"), url);
  },
  
  // requires utilityOverlay.js because it calls openUILinkIn
  openOneBookmark: function (aURI, aTargetBrowser, aDS)
  {
    var url = BookmarksUtils.getProperty(aURI, NC_NS+"URL", aDS);
    // Ignore "NC:" and empty urls.
    if (url == "")
      return;

    openUILinkIn(url, aTargetBrowser);
  },

  openGroupBookmark: function (aURI, aTargetBrowser)
  {
    if (aTargetBrowser == "current" || aTargetBrowser == "tab") {
      var w        = getTopWin();
      var browser  = w.document.getElementById("content");
      var resource = RDF.GetResource(aURI);
      var urlArc   = RDF.GetResource(NC_NS+"URL");
      RDFC.Init(BMDS, resource);
      var containerChildren = RDFC.GetElements();
      var tabPanels = browser.mPanelContainer.childNodes;
      var tabCount  = tabPanels.length;
      var doReplace = PREF.getBoolPref("browser.tabs.loadFolderAndReplace");
      var loadInBackground = PREF.getBoolPref("browser.tabs.loadBookmarksInBackground");
      var index0;
      if (doReplace)
        index0 = 0;
      else {
        for (index0=tabCount-1; index0>=0; --index0)
          if (browser.browsers[index0].webNavigation.currentURI.spec != "about:blank")
            break;
        ++index0;
      }

      var index  = index0;
      while (containerChildren.hasMoreElements()) {
        var res = containerChildren.getNext().QueryInterface(kRDFRSCIID);
        var target = BMDS.GetTarget(res, urlArc, true);
        if (target) {
          var uri = target.QueryInterface(kRDFLITIID).Value;
          if (index < tabCount)
            tabPanels[index].loadURI(uri);
          else
            browser.addTab(uri);
          ++index;
        }
      }

      // If the bookmark group was completely invalid, just bail.
      if (index == index0)
        return;

      // focus the first tab if prefs say to
      if (!loadInBackground || doReplace) {
        // Select the first tab in the group.
        var tabs = browser.mTabContainer.childNodes;
        browser.selectedTab = tabs[index0];
      }

      // Close any remaining open tabs that are left over.
      // (Always skipped when we append tabs)
      for (var i = tabCount-1; i >= index; --i)
        browser.removeTab(tabs[i]);

      // and focus the content
      browser.focus();

    } else {
      dump("Open Group in new window: not implemented...\n");
    }
  },

  createNewBookmark: function (aTarget)
  {
    var name     = BookmarksUtils.getLocaleString("ile_newbookmark");
    var resource = BMSVC.createBookmark(name, "", "", "", "", null);
    this.createNewResource(resource, aTarget, "newbookmark");
  },

  createNewFolder: function (aTarget)
  {
    var name     = BookmarksUtils.getLocaleString("ile_newfolder");
    var resource = BMSVC.createFolder(name);
    this.createNewResource(resource, aTarget, "newfolder");
    // temporary hack...
    return resource;
  },

  createNewSeparator: function (aTarget)
  {
    var resource = BMSVC.createSeparator();
    this.createNewResource(resource, aTarget, "newseparator");
  },

  createNewResource: function(aResource, aTarget, aTxnType)
  {
    var selection = BookmarksUtils.getSelectionFromResource(aResource, aTarget.parent);
    var ok        = BookmarksUtils.insertAndCheckSelection(aTxnType, selection, aTarget);
    if (ok) {
      ok = this.openBookmarkProperties(selection);
      if (!ok)
        BookmarksCommand.deleteBookmark(selection);
    }
  },

  importBookmarks: function ()
  {
    ///transaction...
    try {
      const kFilePickerContractID = "@mozilla.org/filepicker;1";
      const kFilePickerIID = Components.interfaces.nsIFilePicker;
      const kFilePicker = Components.classes[kFilePickerContractID].createInstance(kFilePickerIID);
    
      const kTitle = BookmarksUtils.getLocaleString("SelectImport");
      kFilePicker.init(window, kTitle, kFilePickerIID["modeOpen"]);
      kFilePicker.appendFilters(kFilePickerIID.filterHTML | kFilePickerIID.filterAll);
      var fileName;
      if (kFilePicker.show() != kFilePickerIID.returnCancel) {
        fileName = kFilePicker.file.path;
        if (!fileName) return;
      }
      else return;
    }
    catch (e) {
      return;
    }
    rTarget = RDF.GetResource("NC:BookmarksRoot");
    RDFC.Init(BMDS, rTarget);
    var countBefore = parseInt(BookmarksUtils.getProperty(rTarget, RDF_NS+"nextVal"));
    var args = [{ property: NC_NS+"URL", literal: fileName}];
    this.doBookmarksCommand(rTarget, NC_NS_CMD+"import", args);
    var countAfter = parseInt(BookmarksUtils.getProperty(rTarget, RDF_NS+"nextVal"));

    var transaction = new BookmarkImportTransaction("import");
    for (var index = countBefore; index < countAfter; index++) {
      var nChildArc = RDFCU.IndexToOrdinalResource(index);
      var rChild    = BMDS.GetTarget(rTarget, nChildArc, true);
      transaction.item   .push(rChild);
      transaction.parent .push(rTarget);
      transaction.index  .push(index);
    }
    BMSVC.transactionManager.doTransaction(transaction);
    BookmarksUtils.flushDataSource();
  },

  exportBookmarks: function ()
  {
    try {
      const kFilePickerContractID = "@mozilla.org/filepicker;1";
      const kFilePickerIID = Components.interfaces.nsIFilePicker;
      const kFilePicker = Components.classes[kFilePickerContractID].createInstance(kFilePickerIID);
      
      const kTitle = BookmarksUtils.getLocaleString("EnterExport");
      kFilePicker.init(window, kTitle, kFilePickerIID["modeSave"]);
      kFilePicker.appendFilters(kFilePickerIID.filterHTML | kFilePickerIID.filterAll);
      kFilePicker.defaultString = "bookmarks.html";
      var fileName;
      if (kFilePicker.show() != kFilePickerIID.returnCancel) {
        fileName = kFilePicker.file.path;
        if (!fileName) return;
      }
      else return;
    }
    catch (e) {
      return;
    }
    var selection = RDF.GetResource("NC:BookmarksRoot");
    var args = [{ property: NC_NS+"URL", literal: fileName}];
    this.doBookmarksCommand(selection, NC_NS_CMD+"export", args);
  }

}

  /////////////////////////////////////////////////////////////////////////////
  // Command handling & Updating.
var BookmarksController = {

  supportsCommand: function (aCommand)
  {
    var isCommandSupported;
    switch(aCommand) {
    case "cmd_undo":
    case "cmd_redo":
    case "cmd_bm_undo":
    case "cmd_bm_redo":
    case "cmd_cut":
    case "cmd_copy":
    case "cmd_paste":
    case "cmd_delete":
    case "cmd_selectAll":
    case "cmd_bm_open":
    case "cmd_bm_openinnewwindow":
    case "cmd_bm_openinnewtab":
    case "cmd_bm_expandfolder":
    case "cmd_bm_openfolder":
    case "cmd_bm_managefolder":
    case "cmd_bm_newbookmark":
    case "cmd_bm_newfolder":
    case "cmd_bm_newseparator":
    case "cmd_bm_properties":
    case "cmd_bm_rename":
    case "cmd_bm_setnewbookmarkfolder":
    case "cmd_bm_setpersonaltoolbarfolder":
    case "cmd_bm_setnewsearchfolder":
    case "cmd_bm_import":
    case "cmd_bm_export":
    case "cmd_bm_movebookmark":
      isCommandSupported = true;
      break;
    default:
      isCommandSupported = false;
    }
    //if (!isCommandSupported)
    //  dump("Bookmark command '"+aCommand+"' is not supported!\n");
    return isCommandSupported;
  },

  isCommandEnabled: function (aCommand, aSelection, aTarget)
  {
    var item0, type0, junk;
    var length = aSelection.length;
    if (length != 0) {
      item0 = aSelection.item[0].Value;
      type0 = aSelection.type[0];
    }
    var i;

    switch(aCommand) {
    case "cmd_undo":
    case "cmd_bm_undo":
      return BMSVC.transactionManager.numberOfUndoItems > 0;
    case "cmd_redo":
    case "cmd_bm_redo":
      return BMSVC.transactionManager.numberOfRedoItems > 0;
    case "cmd_paste":
      if (!BookmarksUtils.isValidTargetContainer(aTarget.parent))
        return false;
      const kClipboardContractID = "@mozilla.org/widget/clipboard;1";
      const kClipboardIID = Components.interfaces.nsIClipboard;
      var clipboard = Components.classes[kClipboardContractID].getService(kClipboardIID);
      const kSuppArrayContractID = "@mozilla.org/supports-array;1";
      const kSuppArrayIID = Components.interfaces.nsISupportsArray;
      var flavourArray = Components.classes[kSuppArrayContractID].createInstance(kSuppArrayIID);
      const kSuppStringContractID = "@mozilla.org/supports-cstring;1";
      const kSuppStringIID = Components.interfaces.nsISupportsCString;
    
      var flavours = ["moz/bookmarkclipboarditem", "text/x-moz-url"];
      for (i = 0; i < flavours.length; ++i) {
        const kSuppString = Components.classes[kSuppStringContractID].createInstance(kSuppStringIID);
        kSuppString.data = flavours[i];
        flavourArray.AppendElement(kSuppString);
      }
      var hasFlavours = clipboard.hasDataMatchingFlavors(flavourArray, kClipboardIID.kGlobalClipboard);
      return hasFlavours;
    case "cmd_copy":
      return length > 0;
    case "cmd_cut":
    case "cmd_delete":
      return length > 0 && !aSelection.containsImmutable && !aSelection.containsPTF;
    case "cmd_selectAll":
      return true;
    case "cmd_bm_open":
    case "cmd_bm_expandfolder":
    case "cmd_bm_managefolder":
      return length == 1;
    case "cmd_bm_openinnewwindow":
    case "cmd_bm_openinnewtab":
      return true;
    case "cmd_bm_openfolder":
      for (i=0; i<length; ++i) {
        if (aSelection.type[i] == ""         ||
            aSelection.type[i] == "Bookmark" ||
            aSelection.type[i] == "BookmarkSeparator")
          return false;
        RDFC.Init(BMDS, aSelection.item[i]);
        var children = RDFC.GetElements();
        while (children.hasMoreElements()) {
          if (BookmarksUtils.resolveType(children.getNext()) == "Bookmark")
            return true;
        }
      }
      return false;
    case "cmd_bm_import":
    case "cmd_bm_export":
      return true;
    case "cmd_bm_newbookmark":
    case "cmd_bm_newfolder":
    case "cmd_bm_newseparator":
      return BookmarksUtils.isValidTargetContainer(aTarget.parent);
    case "cmd_bm_properties":
    case "cmd_bm_rename":
      return length == 1;
    case "cmd_bm_setpersonaltoolbarfolder":
      if (length != 1)
        return false;
      return item0 != BMSVC.getBookmarksToolbarFolder().Value && 
             item0 != "NC:BookmarksRoot" && type0 == "Folder";
    case "cmd_bm_movebookmark":
      return length > 0 && !aSelection.containsImmutable;
    default:
      return false;
    }
  },

  doCommand: function (aCommand, aSelection, aTarget)
  {
    switch (aCommand) {
    case "cmd_undo":
    case "cmd_bm_undo":
      BookmarksCommand.undoBookmarkTransaction();
      break;
    case "cmd_redo":
    case "cmd_bm_redo":
      BookmarksCommand.redoBookmarkTransaction();
      break;
    case "cmd_bm_open":
      BookmarksCommand.openBookmark(aSelection, "current");
      break;
    case "cmd_bm_openinnewwindow":
      BookmarksCommand.openBookmark(aSelection, "window");
      break;
    case "cmd_bm_openinnewtab":
      BookmarksCommand.openBookmark(aSelection, "tab");
      break;
    case "cmd_bm_openfolder":
      BookmarksCommand.openBookmark(aSelection, "current");
      break;
    case "cmd_bm_managefolder":
      BookmarksCommand.manageFolder(aSelection);
      break;
    case "cmd_bm_setnewbookmarkfolder":
    case "cmd_bm_setpersonaltoolbarfolder":
    case "cmd_bm_setnewsearchfolder":
      BookmarksCommand.doBookmarksCommand(aSelection.item[0], NC_NS_CMD+aCommand.substring("cmd_bm_".length), []);
      break;
    case "cmd_bm_rename":
    case "cmd_bm_properties":
      junk = BookmarksCommand.openBookmarkProperties(aSelection);
      break;
    case "cmd_cut":
      BookmarksCommand.cutBookmark(aSelection);
      break;
    case "cmd_copy":
      BookmarksCommand.copyBookmark(aSelection);
      break;
    case "cmd_paste":
      BookmarksCommand.pasteBookmark(aTarget);
      break;
    case "cmd_delete":
      BookmarksCommand.deleteBookmark(aSelection);
      break;
    case "cmd_bm_movebookmark":
      BookmarksCommand.moveBookmark(aSelection);
      break;
    case "cmd_bm_newbookmark":
      BookmarksCommand.createNewBookmark(aTarget);
      break;
    case "cmd_bm_newfolder":
      BookmarksCommand.createNewFolder(aTarget);
      break;
    case "cmd_bm_newseparator":
      BookmarksCommand.createNewSeparator(aTarget);
      break;
    case "cmd_bm_import":
      BookmarksCommand.importBookmarks();
      break;
    case "cmd_bm_export":
      BookmarksCommand.exportBookmarks();
      break;
    default: 
      dump("Bookmark command "+aCommand+" not handled!\n");
    }

  },

  onCommandUpdate: function (aSelection, aTarget)
  {
    var commands = ["cmd_bm_newbookmark", "cmd_bm_newfolder", "cmd_bm_newseparator",
                    "cmd_undo", "cmd_redo", "cmd_bm_properties", "cmd_bm_rename", 
                    "cmd_copy", "cmd_paste", "cmd_cut", "cmd_delete",
                    "cmd_bm_setpersonaltoolbarfolder", "cmd_bm_movebookmark", 
                    "cmd_bm_openfolder", "cmd_bm_managefolder"];
    for (var i = 0; i < commands.length; ++i) {
      var enabled = this.isCommandEnabled(commands[i], aSelection, aTarget);
      var commandNode = document.getElementById(commands[i]);
     if (commandNode) { 
        if (enabled) 
          commandNode.removeAttribute("disabled");
        else 
          commandNode.setAttribute("disabled", "true");
      }
    }
  }
}

function CommandArrayEnumerator (aCommandArray)
{
  this._inner = [];
  for (var i = 0; i < aCommandArray.length; ++i)
    this._inner.push(RDF.GetResource(NC_NS_CMD + aCommandArray[i]));
    
  this._index = 0;
}

CommandArrayEnumerator.prototype = {
  getNext: function () 
  {
    return this._inner[this._index];
  },
  
  hasMoreElements: function ()
  {
    return this._index < this._inner.length;
  }
};

var BookmarksUtils = {

  DROP_BEFORE: Components.interfaces.nsITreeView.DROP_BEFORE,
  DROP_ON    : Components.interfaces.nsITreeView.DROP_ON,
  DROP_AFTER : Components.interfaces.nsITreeView.DROP_AFTER,

  _bundle        : null,
  _brandShortName: null,

  /////////////////////////////////////////////////////////////////////////////////////
  // returns a property from chrome://browser/locale/bookmarks/bookmarks.properties
  getLocaleString: function (aStringKey, aReplaceString)
  {
    if (!this._bundle) {
      // for those who would xblify Bookmarks.js, there is a need to create string bundle 
      // manually instead of using <xul:stringbundle/> see bug 63370 for details
      var LOCALESVC = Components.classes["@mozilla.org/intl/nslocaleservice;1"]
                                .getService(Components.interfaces.nsILocaleService);
      var BUNDLESVC = Components.classes["@mozilla.org/intl/stringbundle;1"]
                                .getService(Components.interfaces.nsIStringBundleService);
      var bookmarksBundle  = "chrome://browser/locale/bookmarks/bookmarks.properties";
      this._bundle         = BUNDLESVC.createBundle(bookmarksBundle, LOCALESVC.getApplicationLocale());
      var brandBundle      = "chrome://global/locale/brand.properties";
      this._brandShortName = BUNDLESVC.createBundle(brandBundle,     LOCALESVC.getApplicationLocale())
                                      .GetStringFromName("brandShortName");
    }
   
    var bundle;
    try {
      if (!aReplaceString)
        bundle = this._bundle.GetStringFromName(aStringKey);
      else if (typeof(aReplaceString) == "string")
        bundle = this._bundle.formatStringFromName(aStringKey, [aReplaceString], 1);
      else
        bundle = this._bundle.formatStringFromName(aStringKey, aReplaceString, aReplaceString.length);
    } catch (e) {
      dump("Bookmark bundle "+aStringKey+" not found!\n");
      bundle = "";
    }

    bundle = bundle.replace(/%brandShortName%/, this._brandShortName);
    return bundle;
  },
    
  /////////////////////////////////////////////////////////////////////////////
  // returns the literal targeted by the URI aArcURI for a resource or uri
  getProperty: function (aInput, aArcURI, aDS)
  {
    var node;
    var arc  = RDF.GetResource(aArcURI);
    if (typeof(aInput) == "string") 
      aInput = RDF.GetResource(aInput);
    if (!aDS)
      node = BMDS.GetTarget(aInput, arc, true);
    else
      node = aDS .GetTarget(aInput, arc, true);
    try {
      return node.QueryInterface(kRDFRSCIID).Value;
    }
    catch (e) {
      return node? node.QueryInterface(kRDFLITIID).Value : "";
    }    
  },

  /////////////////////////////////////////////////////////////////////////////
  // Determine the rdf:type property for the given resource.
  resolveType: function (aResource)
  {
    var type = this.getProperty(aResource, RDF_NS+"type");
    if (type != "")
      type = type.split("#")[1];
    if (type == "Folder") {
      if (aResource == BMSVC.getBookmarksToolbarFolder())
        type = "PersonalToolbarFolder";
    }
    return type;
  },

  
  /////////////////////////////////////////////////////////////////////////////
  // Caches frequently used informations about the selection
  checkSelection: function (aSelection)
  {
    if (aSelection.length == 0)
      return;

    aSelection.type        = new Array(aSelection.length);
    aSelection.isContainer = new Array(aSelection.length);
    aSelection.containsPTF = false;
    aSelection.containsImmutable = false;
    var index, item, parent, type, protocol, isContainer, isImmutable;
    for (var i=0; i<aSelection.length; ++i) {
      item        = aSelection.item[i];
      parent      = aSelection.parent[i];
      type        = BookmarksUtils.resolveType(item);
      protocol    = item.Value.split(":")[0];
      isContainer = RDFCU.IsContainer(BMDS, item) ||
                    protocol == "find" || protocol == "file";
      isImmutable = false;
      if (item.Value == "NC:BookmarksRoot") {
        isImmutable = true;
      }
      else if (type != "" && // XXXpch: ugly ugly ugly temporary hack, 
                             // getsynthesizeType says a bookmark is not a
                             // bookmark when it's not a child of sth.
               type != "Bookmark" && type != "BookmarkSeparator" && 
               type != "Folder"   && type != "PersonalToolbarFolder")
        isImmutable = true;
      else if (parent) {
        var parentProtocol = parent.Value.split(":")[0];
        if (parentProtocol == "find" || parentProtocol == "file")
          aSelection.parent[i] = null;
      }
      if (isImmutable)
        aSelection.containsImmutable = true;

      aSelection.type       [i] = type;
      aSelection.isContainer[i] = isContainer;
    }
    if (this.isContainerChildOrSelf(BMSVC.getBookmarksToolbarFolder(), aSelection))
      aSelection.containsPTF = true;
  },

  isSelectionValidForInsertion: function (aSelection, aTarget)
  {
    return BookmarksUtils.isValidTargetContainer(aTarget.parent, aSelection)
  },

  isSelectionValidForDeletion: function (aSelection)
  {
    return !aSelection.containsImmutable;
  },

  /////////////////////////////////////////////////////////////////////////////
  // Returns true is aContainer is a member or a child of the selection
  isContainerChildOrSelf: function (aContainer, aSelection)
  {
    var folder = aContainer;
    do {
      for (var i=0; i<aSelection.length; ++i) {
        if (aSelection.isContainer[i] && aSelection.item[i] == folder)
          return true;
      }
      folder = BMSVC.getParent(folder);
      if (!folder)
        return false; // sanity check
    } while (folder.Value != "NC:BookmarksRoot")
    return false;
  },

  /////////////////////////////////////////////////////////////////////////////
  // Returns true if aSelection can be inserted in aFolder
  isValidTargetContainer: function (aFolder, aSelection)
  {
    if (!aFolder)
      return false;
    if (aFolder.Value == "NC:BookmarksTopRoot")
      return false;
    if (aFolder.Value == "NC:BookmarksRoot")
      return true;

    // don't insert items in an invalid container
    // 'file:' and 'find:' items have a 'Bookmark' type
    var type = BookmarksUtils.resolveType(aFolder);
    if (type != "Folder" && type != "PersonalToolbarFolder")
      return false;

    // bail if we just check the container
    if (!aSelection)
      return true;

    // check that the selected folder is not the selected item nor its child
    if (this.isContainerChildOrSelf(aFolder, aSelection))
      return false;

    return true;
  },

  /////////////////////////////////////////////////////////////////////////////
  removeAndCheckSelection: function (aAction, aSelection)
  {
    isValid = BookmarksUtils.isSelectionValidForDeletion(aSelection);
    if (!isValid) {
      SOUND.beep();
      return false;
    }
    this.removeSelection(aAction, aSelection);
    BookmarksUtils.flushDataSource();
    return true;
  },

  removeSelection: function (aAction, aSelection)
  {
    var transaction    = new BookmarkRemoveTransaction(aAction);
    transaction.item   = [];
    transaction.parent = [];
    transaction.index  = [];
    for (var i = 0; i < aSelection.length; ++i) {
      if (aSelection.parent[i]) {
        RDFC.Init(BMDS, aSelection.parent[i]);
        transaction.item  .push(aSelection.item[i]);
        transaction.parent.push(aSelection.parent[i]);
        transaction.index .push(RDFC.IndexOf(aSelection.item[i]));
      }
    }
    BMSVC.transactionManager.doTransaction(transaction);
    return true;
  },
        
  insertAndCheckSelection: function (aAction, aSelection, aTarget)
  {
    isValid = BookmarksUtils.isSelectionValidForInsertion(aSelection, aTarget);
    if (!isValid) {
      SOUND.beep();
      return false;
    }
    this.insertSelection(aAction, aSelection, aTarget);
    BookmarksUtils.flushDataSource();
    return true;
  },

  insertSelection: function (aAction, aSelection, aTarget)
  {
    var transaction    = new BookmarkInsertTransaction(aAction);
    transaction.item   = new Array(aSelection.length);
    transaction.parent = new Array(aSelection.length);
    transaction.index  = new Array(aSelection.length);
    var index = aTarget.index;
    for (var i=0; i<aSelection.length; ++i) {
      var rSource = aSelection.item[i];
      if (BMSVC.isBookmarkedResource(rSource))
        rSource = BMSVC.cloneResource(rSource);
      transaction.item  [i] = rSource;
      transaction.parent[i] = aTarget.parent;
      transaction.index [i] = index++;
    }
    BMSVC.transactionManager.doTransaction(transaction);
  },

  moveAndCheckSelection: function (aAction, aSelection, aTarget)
  {
    var isValid = BookmarksUtils.isSelectionValidForDeletion(aSelection) &&
                  BookmarksUtils.isSelectionValidForInsertion(aSelection, aTarget);
    if (!isValid) {
      SOUND.beep();
      return false;
    }
    this.moveSelection(aAction, aSelection, aTarget);
    BookmarksUtils.flushDataSource();
    return true;
  },

  moveSelection: function (aAction, aSelection, aTarget)
  {
    var txn = new BookmarkMoveTransaction(aAction, aSelection, aTarget);
    BMSVC.transactionManager.doTransaction(txn);
  }, 

  getXferDataFromSelection: function (aSelection)
  {
    if (aSelection.length == 0)
      return null;
    var dataSet = new TransferDataSet();
    var data, item, itemUrl, itemName, parent, name;
    for (var i=0; i<aSelection.length; ++i) {
      data     = new TransferData();
      item     = aSelection.item[i].Value;
      itemUrl  = this.getProperty(item, NC_NS+"URL");
      itemName = this.getProperty(item, NC_NS+"Name");
      parent   = aSelection.parent[i].Value;
      data.addDataForFlavour("moz/rdfitem",    item+"\n"+(parent?parent:""));
      data.addDataForFlavour("text/x-moz-url", itemUrl+"\n"+itemName);
      data.addDataForFlavour("text/html",      "<A HREF='"+itemUrl+"'>"+itemName+"</A>");
      data.addDataForFlavour("text/unicode",   itemUrl);
      dataSet.push(data);
    }
    return dataSet;
  },

  getSelectionFromXferData: function (aDragSession)
  {
    var selection    = {};
    selection.item   = [];
    selection.parent = [];
    var trans = Components.classes["@mozilla.org/widget/transferable;1"]
                          .createInstance(Components.interfaces.nsITransferable);
    trans.addDataFlavor("moz/rdfitem");
    trans.addDataFlavor("text/x-moz-url");
    trans.addDataFlavor("text/unicode");
    var uri, extra, rSource, rParent, parent;
    for (var i = 0; i < aDragSession.numDropItems; ++i) {
      var bestFlavour = {}, dataObj = {}, len = {};
      aDragSession.getData(trans, i);
      trans.getAnyTransferData(bestFlavour, dataObj, len);
      dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsString);
      if (!dataObj)
        continue;
      dataObj = dataObj.data.substring(0, len.value).split("\n");
      uri     = dataObj[0];
      if (dataObj.length > 1 && dataObj[1] != "")
        extra = dataObj[1];
      else
        extra = null;
      switch (bestFlavour.value) {
      case "moz/rdfitem":
        rSource = RDF.GetResource(uri);
        parent  = extra;
        break;
      case "text/x-moz-url":
      case "text/unicode":
        rSource = BookmarksUtils.createBookmark(null, uri, null, extra, null);
        parent = null;
        break;
      }
      selection.item.push(rSource);
      if (parent)
        rParent = RDF.GetResource(parent);
      else
        rParent = null;
      selection.parent.push(rParent);
    }
    selection.length = selection.item.length;
    BookmarksUtils.checkSelection(selection);
    return selection;
  },

  getTargetFromFolder: function(aResource)
  {
    var index = parseInt(this.getProperty(aResource, RDF_NS+"nextVal"));
    if (isNaN(index))
      return {parent: null, index: -1};
    else
      return {parent: aResource, index: index};
  },

  getSelectionFromResource: function (aItem, aParent)
  {
    var selection    = {};
    selection.length = 1;
    selection.item   = [aItem  ];
    selection.parent = [aParent];
    this.checkSelection(selection);
    return selection;
  },

  createBookmark: function (aName, aURL, aCharSet, aDefaultName)
  {
    if (!aName) {
      // look up in the history ds to retrieve the name
      var rSource = RDF.GetResource(aURL);
      var HISTDS  = RDF.GetDataSource("rdf:history");
      var nameArc = RDF.GetResource(NC_NS+"Name");
      var rName   = HISTDS.GetTarget(rSource, nameArc, true);
      aName       = rName ? rName.QueryInterface(kRDFLITIID).Value : aDefaultName;
      if (!aName)
        aName = aURL;
    }
    if (!aCharSet) {
      var fw = document.commandDispatcher.focusedWindow;
      if (fw)
        aCharSet = fw.document.characterSet;
    }
    return BMSVC.createBookmark(aName, aURL, null, null, aCharSet, null);
  },

  flushDataSource: function ()
  {
    var remoteDS = BMDS.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
    setTimeout(function () {remoteDS.Flush()}, 100);
  },

  // should update the caller, aShowDialog is no more necessary
  addBookmark: function (aURL, aTitle, aCharset, aIsWebPanel)
  {
    openDialog("chrome://browser/content/bookmarks/addBookmark2.xul", "",
               "centerscreen,chrome,dialog,resizable,dependent", aTitle, aURL, null, aCharset,
               null, null, aIsWebPanel);
  }
}

function BookmarkTransaction()
{
}

BookmarkTransaction.prototype = {
  BATCH_LIMIT : 4,
  RDFC        : null,
  BMDS        : null,

  beginUpdateBatch: function()
  {
    if (this.item.length > this.BATCH_LIMIT) {
      this.BMDS.beginUpdateBatch();
    }
  },

  endUpdateBatch: function()
  {
    if (this.item.length > this.BATCH_LIMIT) {
      this.BMDS.endUpdateBatch();
    }
  },
  merge               : function (aTxn)   {return false},
  QueryInterface      : function (aUID)   {return this},
  getHelperForLanguage: function (aCount) {return null},
  getInterfaces       : function (aCount) {return null},
  canCreateWrapper    : function (aIID)   {return "AllAccess"}
}

function BookmarkInsertTransaction (aAction)
{
  this.wrappedJSObject = this;
  this.type    = "insert";
  this.action  = aAction;
  this.item    = null;
  this.parent  = null;
  this.index   = null;
}

BookmarkInsertTransaction.prototype =
{
  __proto__: BookmarkTransaction.prototype,

  isTransient: false,

  doTransaction: function ()
  {
    this.beginUpdateBatch();
    for (var i=0; i<this.item.length; ++i) {
      this.RDFC.Init(this.BMDS, this.parent[i]);
      this.RDFC.InsertElementAt(this.item[i], this.index[i], true);
    }
    this.endUpdateBatch();
  },

  undoTransaction: function ()
  {
    this.beginUpdateBatch();
    // XXXvarga Can't use |RDFC| here because it's being "reused" elsewhere.
    var container = Components.classes[kRDFCContractID].createInstance(kRDFCIID);
    for (var i=this.item.length-1; i>=0; i--) {
      container.Init(this.BMDS, this.parent[i]);
      container.RemoveElementAt(this.index[i], true);
    }
    this.endUpdateBatch();
  },
   
  redoTransaction: function ()
  {
    this.doTransaction();
  }
}

function BookmarkRemoveTransaction (aAction)
{
  this.wrappedJSObject = this;
  this.type    = "remove";
  this.action  = aAction;
  this.item    = null;
  this.parent  = null;
  this.index   = null;
}

BookmarkRemoveTransaction.prototype =
{
  __proto__: BookmarkTransaction.prototype,

  isTransient: false,

  doTransaction: function ()
  {
    this.beginUpdateBatch();
    for (var i=0; i<this.item.length; ++i) {
      this.RDFC.Init(this.BMDS, this.parent[i]);
      this.RDFC.RemoveElementAt(this.index[i], false);
    }
    this.endUpdateBatch();
  },

  undoTransaction: function ()
  {
    this.beginUpdateBatch();
    for (var i=this.item.length-1; i>=0; i--) {
      this.RDFC.Init(this.BMDS, this.parent[i]);
      this.RDFC.InsertElementAt(this.item[i], this.index[i], false);
    }
    this.endUpdateBatch();
  },
   
  redoTransaction: function ()
  {
    this.doTransaction();
  }
}

function BookmarkMoveTransaction (aAction, aSelection, aTarget)
{
  this.wrappedJSObject = this;
  this.type      = "move";
  this.action    = aAction;
  this.selection = aSelection;
  this.target    = aTarget;
}

BookmarkMoveTransaction.prototype =
{
  __proto__: BookmarkTransaction.prototype,

  isTransient: false,

  beginUpdateBatch: function()
  {
    if (this.selection.length > this.BATCH_LIMIT) {
      this.BMDS.beginUpdateBatch();
    }
  },

  endUpdateBatch: function()
  {
    if (this.selection.length > this.BATCH_LIMIT) {
      this.BMDS.endUpdateBatch();
    }
  },

  doTransaction: function ()
  {
    this.beginUpdateBatch();
    BookmarksUtils.removeSelection("move", this.selection);
    BookmarksUtils.insertSelection("move", this.selection, this.target);
    this.endUpdateBatch();
  },

  undoTransaction: function () {},
  redoTransaction: function () {}
}

function BookmarkImportTransaction (aAction)
{
  this.wrappedJSObject = this;
  this.type    = "import";
  this.action  = aAction;
  this.item    = [];
  this.parent  = [];
  this.index   = [];
}

BookmarkImportTransaction.prototype =
{
  __proto__: BookmarkTransaction.prototype,

  isTransient: false,

  doTransaction: function ()
  {
  },

  undoTransaction: function ()
  {
    this.beginUpdateBatch();
    for (var i=this.item.length-1; i>=0; i--) {
      this.RDFC.Init(this.BMDS, this.parent[i]);
      this.RDFC.RemoveElementAt(this.index[i], true);
    }
    this.endUpdateBatch();
  },
   
  redoTransaction: function ()
  {
    this.beginUpdateBatch();
    for (var i=0; i<this.item.length; ++i) {
      this.RDFC.Init(this.BMDS, this.parent[i]);
      this.RDFC.InsertElementAt(this.item[i], this.index[i], true);
    }
    this.endUpdateBatch();
  }
}

var BookmarkEditMenuTxnListener =
{

  didDo: function (aTxmgr, aTxn)
  {
    this.updateMenuItem(aTxmgr, aTxn);
  },

  didUndo: function (aTxmgr, aTxn)
  {
    this.updateMenuItem(aTxmgr, aTxn);
  },

  didRedo: function (aTxmgr, aTxn)
  {
    this.updateMenuItem(aTxmgr, aTxn);
  },

  didMerge       : function (aTxmgr, aTxn) {},
  didBeginBatch  : function (aTxmgr, aTxn) {},
  didEndBatch    : function (aTxmgr, aTxn) {},
  willDo         : function (aTxmgr, aTxn) {},
  willUndo       : function (aTxmgr, aTxn) {},
  willRedo       : function (aTxmgr, aTxn) {},
  willMerge      : function (aTxmgr, aTxn) {},
  willBeginBatch : function (aTxmgr, aTxn) {},
  willEndBatch   : function (aTxmgr, aTxn) {},

  updateMenuItem: function (aTxmgr, aTxn) {
    if (aTxn) {
      aTxn = aTxn.wrappedJSObject;
      if ((aTxn.type == "remove" || aTxn.type == "insert") && aTxn.action == "move")
      return;
    }
    var node, transactionNumber, transactionList, transactionLabel, action;
    node = document.getElementById("cmd_undo");
    transactionNumber = aTxmgr.numberOfUndoItems;
    dump("N UNDO: "+transactionNumber+"\n")
    if (transactionNumber == 0) {
      transactionLabel = BookmarksUtils.getLocaleString("cmd_bm_undo");
    } else {
      transactionList  = aTxmgr.getUndoList();
      action           = transactionList.getItem(transactionNumber-1).wrappedJSObject.action;
      transactionLabel = BookmarksUtils.getLocaleString("cmd_bm_"+action+"_undo")
    }
    node.setAttribute("label", transactionLabel);
      
    node = document.getElementById("cmd_redo");
    transactionNumber = aTxmgr.numberOfRedoItems;
    dump("N REDO: "+transactionNumber+"\n")
    if (transactionNumber == 0) {
      transactionLabel = BookmarksUtils.getLocaleString("cmd_bm_redo");
    } else {
      transactionList  = aTxmgr.getRedoList();
      action           = transactionList.getItem(transactionNumber-1).wrappedJSObject.action;
      transactionLabel = BookmarksUtils.getLocaleString("cmd_bm_"+action+"_redo")
    }
    node.setAttribute("label", transactionLabel);
  }
}

#ifdef 0

var _dumpTIME;
function dumpTIME(aString)
{
  var now=Date.now();
  dump(aString);
  if (_dumpTIME)
    dump(now-_dumpTIME+'ms\n');
  else
    _dumpTIME = now;
}

function dumpOBJ (aObj) 
{
  if (!aObj)
    dump("*** null object\n");
  for (var i in aObj) {
    dump("*** aObj[" + i + "] = ");
    try {
      dump(aObj[i] + "\n");
    } catch (e) { 
      dump("*** not available\n")
    }
  }
}

function dumpDOM (aElement, aIndent)
{
  if (!aElement)
    return;
  if (typeof(aElement) == "string")
    aElement = document.getElementById(aElement);

  if (!aIndent)
    aIndent = 0;
  for (var i=0; i<aIndent*2; ++i)
    dump("-");
  dump("-> ");
  dump(aElement.localName+ " ("+aElement.id+') "'+aElement.getAttribute("class")+'"\n');
  var element = aElement.firstChild;
  while (element) {
    dumpDOM(element,++aIndent);
    --aIndent;
    element = element.nextSibling;
  }
}

function dumpRDF (aDS, aRDFNode)
{
  dumpRDFOut(aDS, aRDFNode);
  dump("\n");
  dumpRDFIn (aDS, aRDFNode);
}

function dumpRDFOut (aDS, aRDFNode)
{
  dump("Arcs Out for "+aRDFNode.Value+"\n");
  var arcsout=aDS.ArcLabelsOut(aRDFNode);
  while (arcsout.hasMoreElements()) {
    var arc = arcsout.getNext().QueryInterface(Components.interfaces.nsIRDFResource);
    var targets = aDS.GetTargets(aRDFNode, arc, true);
    while (targets.hasMoreElements()) {
      var target = targets.getNext();
      try {
        target = target.QueryInterface(Components.interfaces.nsIRDFResource).Value;
      } catch (e) {
        try {
          target = target.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
        } catch (e) {}
      }
      dump("=>"+arc.Value+"=>"+target+"\n");
    }
  }
}
function dumpRDFIn (aDS, aRDFNode)
{
  dump("Arcs In for "+aRDFNode.Value+"\n");
  var arcs=aDS.ArcLabelsIn(aRDFNode);
  while (arcs.hasMoreElements()) {
    var arc = arcs.getNext().QueryInterface(Components.interfaces.nsIRDFResource);
    var sources = aDS.GetSources(arc, aRDFNode, true);
    while (sources.hasMoreElements()) {
      var source = sources.getNext();
      try {
        source = source.QueryInterface(Components.interfaces.nsIRDFResource).Value;
      } catch (e) {
        try {
          source = source.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
        } catch (e) {}
      }
      dump("<="+arc.Value+"<="+source+"\n");
    }
  }
}

function dumpTXN(aTxn)
{
  aTxn = aTxn.wrappedJSObject;
  dump(aTxn.type+", "+aTxn.action+"\n");
  if (aTxn.type == "insert" || aTxn.type == "remove") {
    for (var i=0; i<aTxn.item.length; ++i) {
      dump(i+": "+aTxn.item[i].Value+" in "+BookmarksUtils.getProperty(aTxn.parent[i], NC_NS+"Name")+", i:"+aTxn.index[i]+"\n");
    }
  }
}
#endif
