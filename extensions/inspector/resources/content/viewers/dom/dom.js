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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 *
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

/***************************************************************
* DOMViewer --------------------------------------------
*  Views all nodes within a document.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/util.js
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
*   chrome://inspector/content/jsutil/rdf/RDFU.js
****************************************************************/

//////////// global variables /////////////////////

var viewer;

window.onunload = function() { viewer.destroy(); }

var gColumnExtras = {
  "Anonymous": "Anonymous", 
  "nodeType": "nodeType"
};

var gColumnAttrs = {
  "class": "triDOMView"
};

//////////// global constants ////////////////////

const kDOMDataSourceIID           = "@mozilla.org/rdf/datasource;1?name=Inspector_DOM";
const kClipboardHelperCID  = "@mozilla.org/widget/clipboardhelper;1";
const kGlobalClipboard     = Components.interfaces.nsIClipboard.kGlobalClipboard;

//////////////////////////////////////////////////

window.addEventListener("load", DOMViewer_initialize, false);

function DOMViewer_initialize()
{
  viewer = new DOMViewer();
  viewer.initialize(parent.FrameExchange.receiveData(window));
}

////////////////////////////////////////////////////////////////////////////
//// class DOMViewer 

function DOMViewer() // implements inIViewer
{
  this.mObsMan = new ObserverManager(this);
    
  this.mDOMOutliner = document.getElementById("trDOMOutliner");
  this.mDOMOutlinerBody = document.getElementById("trDOMOutlinerBody");

  // prepare and attach the DOM DataSource
  this.mDOMDS = XPCU.createInstance(kDOMDataSourceIID, "inIDOMDataSource");
  this.mDOMDS.showSubDocuments = true;
  this.mDOMDS.removeFilterByType(2); // hide attribute nodes
  this.mDOMOutlinerBody.database.AddDataSource(this.mDOMDS);

  PrefUtils.addObserver("inspector", PrefChangeObserver);
}

DOMViewer.prototype = 
{
  mSubject: null,
  mDOMDS: null,
  // searching stuff
  mSearchResults: null,
  mSearchCurrentIdx: null,
  mSearchDirection: null,
  mColumns: null,
  mFlashSelected: null,
  mFlashes: 0,
  
  ////////////////////////////////////////////////////////////////////////////
  //// interface inIViewer

  //// attributes 

  get uid() { return "dom" },
  get pane() { return this.mPanel },

  get selection() { return this.mSelection },

  get subject() { return this.mSubject },
  set subject(aObject) {
    this.mSubject = aObject;
    this.mDOMDS.document = aObject;
    try {
      this.mOutlinerBuilder.buildContent();
      if (this.mPanel.params)
        this.selectElementInOutliner(this.mPanel.params);
      else
        this.selectElementInOutliner(aObject.documentElement, true);
    } catch (ex) {
      debug("ERROR: While rebuilding dom tree\n" + ex);    
    }
    this.mObsMan.dispatchEvent("subjectChange", { subject: aObject });
  },

  //// methods

  initialize: function(aPane)
  {
    this.initColumns();

    this.mPanel = aPane;
    aPane.notifyViewerReady(this);

    this.toggleAnonContent(true, PrefUtils.getPref("inspector.dom.showAnon"));
    this.setFlashSelected(PrefUtils.getPref("inspector.blink.on"));
  },
  
  destroy: function()
  {
    if (this.mEvilListener && "setListener" in this.mEvilListener)
      this.mEvilListener.setListener(null);
  },

  ////////////////////////////////////////////////////////////////////////////
  //// event dispatching

  addObserver: function(aEvent, aObserver) { this.mObsMan.addObserver(aEvent, aObserver); },
  removeObserver: function(aEvent, aObserver) { this.mObsMan.removeObserver(aEvent, aObserver); },
  
  ////////////////////////////////////////////////////////////////////////////
  //// UI Commands
  
  showFindDialog: function()
  {
    var win = openDialog("chrome://inspector/content/viewers/dom/findDialog.xul", 
                         "_blank", "chrome,dependent", this.mFindType, this.mFindDir, this.mFindParams);
  },

  toggleAnonContent: function(aExplicit, aValue)
  {
    var val = aExplicit ? aValue : !this.mDOMDS.showAnonymousContent;
    this.mDOMDS.showAnonymousContent = val;
    this.mPanel.panelset.setCommandAttribute("cmd:toggleAnon", "checked", val ? "true" : "false");
    PrefUtils.setPref("inspector.dom.showAnon", val);
  },
  
  toggleSubDocs: function(aExplicit, aValue)
  {
    var val = aExplicit ? aValue : !this.mDOMDS.showSubDocuments;
    this.mDOMDS.showSubDocuments = val;
    this.mPanel.panelset.setCommandAttribute("cmd:toggleSubDocs", "checked", val ? "true" : "false");
  },
  
  toggleAttributes: function(aExplicit, aValue)
  {
    alert("NOT YET IMPLEMENTED");
  },
  
  showColumnsDialog: function()
  {
    var win = openDialog("chrome://inspector/content/viewers/dom/columnsDialog.xul", 
      "_blank", "chrome,dependent", this);
  },

  cmdShowPseudoClasses: function()
  {
    var idx = this.mDOMOutliner.currentIndex;
    var node = this.getNodeFromRowIndex(idx);

    var win = openDialog("chrome://inspector/content/viewers/dom/pseudoClassDialog.xul", 
                         "_blank", "chrome", node);
  },

  onItemSelected: function()
  {
    var idx = this.mDOMOutliner.currentIndex;
    this.mSelection = this.getNodeFromRowIndex(idx);
    this.mObsMan.dispatchEvent("selectionChange", { selection: this.mSelection } );

    if (this.mFlashSelected)
      this.flashElement(this.mSelection);
  },
  
  onContextCreate: function(aPP)
  {
    var mi, cmd;
    for (var i = 0; i < aPP.childNodes.length; ++i) {
      mi = aPP.childNodes[i];
      if (mi.hasAttribute("observes")) {
        cmd = document.getElementById(mi.getAttribute("observes"));
        if (cmd && cmd.hasAttribute("isvalid")) {
          try {
            var isValid = new Function(cmd.getAttribute("isvalid"));
          } catch (ex) { /* die quietly on syntax error in handler */ }
          if (!isValid())
            mi.setAttribute("hidden", "true");
          else
            mi.removeAttribute("hidden");
        }
      }
    }
  },
  
  cmdDeleteSelectedNode: function()
  {
    var node = this.getSelectedNode();
    if (node)
      node.parentNode.removeChild(node);
  },
  
  cmdInspectBrowserIsValid: function()
  {
    var node = viewer.getSelectedNode();
    if (!node) return false;
    
    var n = node.localName.toLowerCase();
    return n == "browser" || n == "iframe" || n == "frame" || n == "editor";
  },
  
  cmdInspectBrowser: function()
  {
    var node = this.getSelectedNode();
    var n = node.localName.toLowerCase();
    if (node && n == "browser" && node.namespaceURI == kXULNSURI) {
      // xul browser
      this.subject = node.contentDocument;
    } else if (n == "iframe" && node.namespaceURI == kXULNSURI) {
      // xul iframe
      this.subject = node.contentDocument;
    } else if (n == "iframe" || n == "frame") {
      // html iframe or frame
      this.subject = node.contentDocument;
    } else if (n == "editor") {
      // editor shell
      this.subject = node.editorShell.editorDocument;
    }
  },
 
  cmdInspectInNewWindow: function()
  {
    var node = this.getSelectedNode();
    inspectObject(node);
  },
  
  ////////////////////////////////////////////////////////////////////////////
  //// XML Serialization

  cmdCopySelectedXML: function()
  {
    var node = this.getSelectedNode();
    if (node) {
      var xml = this.toXML(node);
    
      var helper = XPCU.getService(kClipboardHelperCID, "nsIClipboardHelper");
      helper.copyStringToClipboard(xml, kGlobalClipboard);    
    }
  },

  toXML: function(aNode)
  {
    return this._toXML(aNode, 0);
  },
  
  // not the most complete serialization ever conceived, but it'll do for now
  _toXML: function(aNode, aLevel)
  {
    if (!aNode) return "";
    
    var s = "";
    var indent = "";
    for (var i = 0; i < aLevel; ++i)
      indent += "  ";
    var line = indent;        
      
    if (aNode.nodeType == 1) {
      line += "<" + aNode.localName;

      var attrIndent = "";
      for (i = 0; i < line.length; ++i)
        attrIndent += " ";
  
      for (i = 0; i < aNode.attributes.length; ++i) {
        var a = aNode.attributes[i];
        var attr = " " + a.localName + "=\"" + a.nodeValue + "\"";
        if (line.length + attr.length > 80) {
          s += line + (i < aNode.attributes.length-1 ? "\n"+attrIndent : "");
          line = "";
        }
        
        line += attr;
      }
      s += line;
      
      if (aNode.childNodes.length == 0)
        s += "/>\n";
      else {
        s += ">\n";
        for (i = 0; i < aNode.childNodes.length; ++i)
          s += this._toXML(aNode.childNodes[i], aLevel+1);
        s += indent + "</" + aNode.localName + ">\n";
      }
    } else if (aNode.nodeType == 3) {
      s += aNode.nodeValue;
    } else if (aNode.nodeType == 8) {
      s += line + "<!--" + aNode.nodeValue + "-->\n";
    }
    
    return s;
  },
  
  ////////////////////////////////////////////////////////////////////////////
  //// Click Selection

  selectByClick: function()
  {
    if (this.mSelecting) {
      this.stopSelectByClick();
    } else {
      // wait until after user releases the mouse after selecting this command from a UI element
      window.setTimeout("viewer.startSelectByClick()", 10);
    }
  },
  
  startSelectByClick: function()
  {
    this.mSelecting = true;
    this.mSelectDocs = this.getAllDocuments();

    for (var i = 0; i < this.mSelectDocs.length; ++i)
      this.mSelectDocs[i].addEventListener("mousedown", MouseDownListener, true);

    this.mPanel.panelset.setCommandAttribute("cmd:selectByClick", "checked", "true");
  },

  selectByClickOver: function(aTarget)
  {
    if (this.mLastOver)
      this.flasher.stop();

    this.flasher.element = aTarget;
    this.flasher.start(-1, 1, true);
    
    this.mLastOver = aTarget;
  },
  
  doSelectByClick: function(aTarget)
  {
    this.stopSelectByClick();
    this.selectElementInOutliner(aTarget);
  },

  stopSelectByClick: function()
  {
    this.mSelecting = false;

    for (var i = 0; i < this.mSelectDocs.length; ++i)
      this.mSelectDocs[i].removeEventListener("mousedown", MouseDownListener, true);

    this.mPanel.panelset.setCommandAttribute("cmd:selectByClick", "checked", null);
  },

  ////////////////////////////////////////////////////////////////////////////
  //// Find Methods

  startFind: function(aType, aDir)
  {
    this.mFindType = aType;
    this.mFindDir = aDir;
    this.mFindParams = [];
    for (var i = 2; i < arguments.length; ++i)
      this.mFindParams[i-2] = arguments[i];
      
    var fn = null;
    switch (aType) {
      case "id":
        fn = "doFindElementById";
        break;
      case "tag":
        fn = "doFindElementsByTagName";
        break;
      case "attr":
        fn = "doFindElementsByAttr";
        break;
    };

    this.mFindFn = fn;
    this.mFindWalker = this.createDOMWalker(this.mDOMDS.document.documentElement);
    this.findNext();
  },
  
  findNext: function()
  {
    var walker = this.mFindWalker;
    var result = null;
    if (walker) {
      while (walker.currentNode) {
        //dump((walker.currentNode ? walker.currentNode.localName : "") + "\n");
        if (this[this.mFindFn](walker)) {
          result = walker.currentNode;
          walker.nextNode();
          break;
        }
        walker.nextNode();
      }
      
      if (result) {
        this.selectElementInOutliner(result);
        this.mDOMOutliner.focus();
      } else {
        alert("End of document reached."); // XXX localize
      }
    }
  },

  doFindElementById: function(aWalker)
  {
    return aWalker.currentNode && aWalker.currentNode.id == this.mFindParams[0];
  },

  doFindElementsByTagName: function(aWalker)
  {
    return aWalker.currentNode && aWalker.currentNode.localName.toLowerCase() == this.mFindParams[0].toLowerCase();
  },

  doFindElementsByAttr: function(aWalker)
  {
    return aWalker.currentNode && 
           aWalker.currentNode.getAttribute(this.mFindParams[0]) == this.mFindParams[1];
  },
  
  ///////////////////////////////////////////////////////////////////////////
  // Takes an element from the document being inspected, finds the treeitem
  // which represents it in the DOM tree and selects that treeitem.
  //
  // @param aEl - element from the document being inspected
  ///////////////////////////////////////////////////////////////////////////

  selectElementInOutliner: function(aEl, aExpand)
  {
    // Keep searching until a pre-created ancestor is
    // found, and then open each ancestor until
    // the found element is created
    var walker = this.createDOMWalker(aEl);
    var line = [];
    var parent = aEl;
    var index = null;
    while (parent) {
      index = this.getRowIndexFromNode(parent);
      line.push(parent);
      if (index < 0) { 
        // row for this node hasn't been created yet
        parent = walker.parentNode();
      } else
        break;
    } 
  
    var bx = this.mDOMOutliner.boxObject.QueryInterface(Components.interfaces.nsIOutlinerBoxObject);
    var view = bx.view;

    // we've got all the ancestors, now open them 
    // one-by-one from the top on down
    for (var i = line.length-1; i >= 0; i--) {
      index = this.getRowIndexFromNode(line[i]);
      if (index < 0) return false; // can't find row, so stop trying to descend
      if ((aExpand || i > 0) && !view.isContainerOpen(index))
        view.toggleOpenState(index);
      if (i == 0)
        bx.ensureRowIsVisible(index);    
    }

    bx.selection.select(index);
  },
  
  createDOMWalker: function(aRoot)
  {
    var walker = XPCU.createInstance("@mozilla.org/inspector/deep-tree-walker;1", "inIDeepTreeWalker");
    walker.showAnonymousContent = this.mDOMDS.showAnonymousContent;
    walker.showSubDocuments = this.mDOMDS.showSubDocuments;
    walker.init(aRoot, Components.interfaces.nsIDOMNodeFilter.SHOW_ALL);
    return walker;
  },
  
  ////////////////////////////////////////////////////////////////////////////
  //// Columns

  initColumns: function()
  {
    var colPref = PrefUtils.getPref("inspector.dom.columns");
    var cols = colPref.split(",")
    this.mColumns = cols;
    this.mColumnHash = {};
      
    var tb = new inOutlinerBuilder(this.mDOMOutliner, kInspectorNSURI, "Child");
    tb.allowDragColumns = true;
    tb.isRefContainer = false;
    tb.isContainer = true;
    tb.rowAttributes = gColumnAttrs;
    tb.rowFields = gColumnExtras;
    this.mOutlinerBuilder = tb;

    tb.initialize();
    
    for (var i = 0; i < cols.length; i++) {
      this.mColumnHash[cols[i]] = true;
      tb.addColumn({
        name: cols[i], 
        title: cols[i],
        flex: i == 0 ? 2 : 1,
        className: "triDOMView"});
    }
    
    // start listening for modifications to columns
    // from drag and drop operations
    tb.onColumnAdd = gColumnAddListener;
    tb.onColumnRemove = gColumnRemoveListener;
    
    tb.build();
  },
  
  hasColumn: function(aName)
  {
    return this.mColumnHash[aName] == true;
  },
  
  //// these add/remove methods depend on names that have already 
  //// been added to the tree builder
  
  doAddColumn: function(aIndex)
  {
    var name = this.mOutlinerBuilder.getColumnName(aIndex);
    this.mColumnHash[name] = true;
    this.mColumns.splice(aIndex, 0, name);
    this.saveColumns();
  },
  
  doRemoveColumn: function(aIndex)
  {
    var name = this.mColumns[aIndex];
    this.mColumnHash[name] = null;
    this.mColumns.splice(aIndex, 1);
    this.saveColumns();
  },
  
  onColumnAdd: function(aIndex)
  {
    this.doAddColumn(aIndex);
  },
  
  onColumnRemove: function(aIndex)
  {
    this.doRemoveColumn(aIndex);
  },
  
  saveColumns: function()
  {
    var cols = this.mColumns.join(",");
    PrefUtils.setPref("inspector.dom.columns", cols);
  },

  onColumnsDialogReady: function (aDialog)
  {
    this.mColumnsDialog = aDialog;
    this.mOutlinerBuilder.addColumnDropTarget(aDialog.box);
  },
  
  onColumnsDialogClose: function (aDialog)
  {
    this.mColumnsDialog = null;
  },

  ////////////////////////////////////////////////////////////////////////////
  //// Flashing

  get flasher()
  {
    if (!("mFlasher" in this)) {
      this.mFlasher = new Flasher(PrefUtils.getPref("inspector.blink.border-color"), 
                                  PrefUtils.getPref("inspector.blink.border-width"), 
                                  PrefUtils.getPref("inspector.blink.duration"), 
                                  PrefUtils.getPref("inspector.blink.speed"));
    }
    
    return this.mFlasher;
  },

  flashElement: function(aElement)
  {
    // make sure we only try to flash element nodes
    if (aElement.nodeType == 1) {
      var flasher = this.flasher;
      
      if (flasher.flashing) 
        flasher.stop();
        
      try {
        flasher.element = aElement;
        flasher.start();
      } catch (ex) {
      }
    }
  },

  toggleFlashSelected: function(aExplicit, aValue)
  {
    var val = aExplicit ? aValue : !this.mFlashSelected;
    this.setFlashSelected(val);
  },

  setFlashSelected: function(aValue)
  {
    this.mFlashSelected = aValue;
    this.mPanel.panelset.setCommandAttribute("cmd:flashSelected", "checked", aValue);
    PrefUtils.setPref("inspector.blink.on", aValue);
  },

  ////////////////////////////////////////////////////////////////////////////
  //// Prefs

  onPrefChanged: function(aName)
  {
    if (aName == "inspector.dom.showAnon")
      this.setFlashSelected(PrefUtils.getPref("inspector.blink.on"));

    if (aName == "inspector.blink.on")
      this.setFlashSelected(PrefUtils.getPref("inspector.blink.on"));

    if (this.mFlasher) {
      if (aName == "inspector.blink.border-color") {
        this.mFlasher.color = PrefUtils.getPref("inspector.blink.border-color");
      } else if (aName == "inspector.blink.border-width") {
        this.mFlasher.thickness = PrefUtils.getPref("inspector.blink.border-width");
      } else if (aName == "inspector.blink.duration") {
        this.mFlasher.duration = PrefUtils.getPref("inspector.blink.duration");
      } else if (aName == "inspector.blink.speed") {
        this.mFlasher.speed = PrefUtils.getPref("inspector.blink.speed");
      }
    }
  },

  ////////////////////////////////////////////////////////////////////////////
  //// Uncategorized
  
  getAllDocuments: function()
  {
    var doc = this.mDOMDS.document;
    var results = [doc];
    this.findDocuments(doc, results);
    return results;
  },
  
  findDocuments: function(aDoc, aArray)
  {
    this.addKidsToArray(aDoc.getElementsByTagName("frame"), aArray);
    this.addKidsToArray(aDoc.getElementsByTagName("iframe"), aArray);
    this.addKidsToArray(aDoc.getElementsByTagNameNS(kXULNSURI, "browser"), aArray);
    this.addKidsToArray(aDoc.getElementsByTagNameNS(kXULNSURI, "editor"), aArray);
  },
  
  addKidsToArray: function(aKids, aArray)
  {
    for (var i = 0; i < aKids.length; ++i) {
      try {
        if (aKids.localName == "editor")
          aArray.push(aKids[i].editorShell.editorDocument);
        else
          aArray.push(aKids[i].contentDocument);
      } catch (ex) {
        // if we can't access the content document, skip it
      }
    }
  },
  
  getNodeFromRowIndex: function(aIndex)
  {
    if (aIndex < 0)
      return null;
    var builder = this.mDOMOutlinerBody.builder.QueryInterface(Components.interfaces.nsIXULOutlinerBuilder);
    var res = builder.getResourceAtIndex(aIndex);
    res = res.QueryInterface(Components.interfaces.inIDOMRDFResource);
    return res ? res.object : null;
  },
  
  getRowIndexFromNode: function(aNode)
  {
    var builder = this.mDOMOutlinerBody.builder.QueryInterface(Components.interfaces.nsIXULOutlinerBuilder);
    var res = this.mDOMDS.getResourceForObject(aNode);
    return res ? builder.getIndexOfResource(res) : -1;
  },
  
  getSelectedNode: function()
  {
    var sel = this.mDOMOutliner.currentIndex;
    return this.getNodeFromRowIndex(sel);
  }

};

////////////////////////////////////////////////////////////////////////////
//// Listener Objects

var MouseDownListener = {
  handleEvent: function(aEvent)
  {
    if (aEvent.type == "mousedown")
      viewer.doSelectByClick(aEvent.target);
    else if (aEvent.type == "mouseover")
      viewer.selectByClickOver(aEvent.target);
  }
}

var PrefChangeObserver = {
  Observe: function(aSubject, aTopic, aData)
  {
    viewer.onPrefChanged(aData);
  }
};

function gColumnAddListener(aIndex)
{
  viewer.onColumnAdd(aIndex);
}

function gColumnRemoveListener(aIndex)
{
  viewer.onColumnRemove(aIndex);
}


function dumpDOM2(aNode)
{
  dump(DOMViewer.prototype.toXML(aNode));
}

