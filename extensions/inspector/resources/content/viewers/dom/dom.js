/* ***** BEGIN LICENSE BLOCK *****
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

/***************************************************************
* DOMViewer --------------------------------------------
*  Views all nodes within a document.
****************************************************************/

//////////// global variables /////////////////////

var viewer;
var gEntityConverter;

//////////// global constants ////////////////////

const kDOMViewCID          = "@mozilla.org/inspector/dom-view;1";
const kClipboardHelperCID  = "@mozilla.org/widget/clipboardhelper;1";
const kPromptServiceCID    = "@mozilla.org/embedcomp/prompt-service;1";

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
    
  this.mDOMTree = document.getElementById("trDOMTree");
  this.mDOMTreeBody = document.getElementById("trDOMTreeBody");

  // prepare and attach the DOM DataSource
  this.mDOMView = XPCU.createInstance(kDOMViewCID, "inIDOMView");
  this.mDOMView.showSubDocuments = true;
  this.mDOMView.whatToShow &= ~(NodeFilter.SHOW_ATTRIBUTE); // hide attribute nodes
  this.mDOMTree.treeBoxObject.view = this.mDOMView;

  PrefUtils.addObserver("inspector", PrefChangeObserver);
}

DOMViewer.prototype = 
{
  mSubject: null,
  mDOMView: null,
  // searching stuff
  mSearchResults: null,
  mSearchCurrentIdx: null,
  mSearchDirection: null,
  mColumns: null,
  mFlashSelected: null,
  mFlashes: 0,
  mFindDir: null,
  mFindParams: null,
  mFindType: null,
  mFindWalker: null,
  mSelecting: false,
  
  ////////////////////////////////////////////////////////////////////////////
  //// interface inIViewer

  //// attributes 

  get uid() { return "dom" },
  get pane() { return this.mPanel },
  get editable() { return true; },
  
  get selection() { return this.mSelection },

  get subject() { return this.mSubject },
  set subject(aObject) {
    this.mSubject = aObject;
    this.mDOMView.rootNode = aObject;
    this.mObsMan.dispatchEvent("subjectChange", { subject: aObject });
    this.setInitialSelection(aObject);
  },

  //// methods

  initialize: function(aPane)
  {
    //this.initColumns();

    this.mPanel = aPane;
    aPane.notifyViewerReady(this);

    this.setAnonContent(PrefUtils.getPref("inspector.dom.showAnon"), false);
    this.setWhitespaceNodes(PrefUtils.getPref("inspector.dom.showWhitespaceNodes"));
    this.setFlashSelected(PrefUtils.getPref("inspector.blink.on"));
  },

  destroy: function()
  {
    this.mDOMTree.treeBoxObject.view = null;
  },

  isCommandEnabled: function(aCommand)
  {
    switch (aCommand) {
      case "cmdEditPaste":
        var canPaste = this.mPanel.panelset.clipboardFlavor == "inspector/dom-node";
        if (canPaste) {
          var node = this.mPanel.panelset.getClipboardData();
          canPaste = node ? node.nodeType != Node.ATTRIBUTE_NODE : false;
        }
        return canPaste;
      case "cmdEditInsert":
        return false;
      case "cmdEditCut":
      case "cmdEditCopy":
      case "cmdEditDelete":
        return this.selectedNode != null;
    }
    return false;
  },
  
  getCommand: function(aCommand)
  {
    switch (aCommand) {
      case "cmdEditCut":
        return new cmdEditCut();
      case "cmdEditCopy":
        return new cmdEditCopy();
      case "cmdEditPaste":
        return new cmdEditPaste();
      case "cmdEditDelete":
        return new cmdEditDelete();
    }
    return null;
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

  toggleAnonContent: function(aRebuild)
  {
    this.setAnonContent(!this.mDOMView.showAnonymousContent, aRebuild);
  },
  
  setAnonContent: function(aValue, aRebuild)
  {
    this.mDOMView.showAnonymousContent = aValue;
    this.mPanel.panelset.setCommandAttribute("cmd:toggleAnon", "checked", aValue);
    PrefUtils.setPref("inspector.dom.showAnon", aValue);

    if (aRebuild) {
      this.rebuild();
    }
  },

  toggleSubDocs: function()
  {
    var val = !this.mDOMView.showSubDocuments;
    this.mDOMView.showSubDocuments = val;
    this.mPanel.panelset.setCommandAttribute("cmd:toggleSubDocs", "checked", val);
  },

  setWhitespaceNodes: function(aValue)
  {
    // Do this first so we ensure the checkmark is set in the case
    // we are starting with whitespace nodes enabled.
    this.mPanel.panelset.setCommandAttribute("cmd:toggleWhitespaceNodes", "checked", aValue);

    // The rest of the stuff is redundant to do if we are not changing
    // the value, so just bail here if we're setting the same value.
    if (this.mDOMView.showWhitespaceNodes == aValue) {
      return;
    }

    this.mDOMView.showWhitespaceNodes = aValue;
    PrefUtils.setPref("inspector.dom.showWhitespaceNodes", aValue);
    this.rebuild();
  },

  toggleWhitespaceNodes: function()
  {
    this.setWhitespaceNodes(!this.mDOMView.showWhitespaceNodes);
  },

  toggleAttributes: function()
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
    var idx = this.mDOMTree.currentIndex;
    var node = this.getNodeFromRowIndex(idx);
    
    if (node)
      openDialog("chrome://inspector/content/viewers/dom/pseudoClassDialog.xul", 
                           "_blank", "chrome", node);
  },

  cmdBlink: function()
  {
    this.flashElement(this.selectedNode);
  },
  
  cmdBlinkIsValid: function()
  {
    return this.selectedNode ? (this.selectedNode.nodeType == Node.ELEMENT_NODE) : false;
  },
    
  onItemSelected: function()
  {
    var idx = this.mDOMTree.currentIndex;
    this.mSelection = this.getNodeFromRowIndex(idx);
    this.mObsMan.dispatchEvent("selectionChange", { selection: this.mSelection } );
  
    if (this.mSelection) {
      if (this.mFlashSelected)
        this.flashElement(this.mSelection);
    }
  },
  
  setInitialSelection: function(aObject)
  {
    var fireSelected = this.mDOMTree.currentIndex == 0;
  
    if (this.mPanel.params)
      this.selectElementInTree(this.mPanel.params);
    else
      this.selectElementInTree(aObject, true);
  
    if (fireSelected)
      this.onItemSelected();
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
  
  cmdInspectBrowserIsValid: function()
  {
    var node = viewer.selectedNode;
    if (!node || node.nodeType != Node.ELEMENT_NODE) return false;
    
    var n = node.localName.toLowerCase();
    return n == "tabbrowser" || n == "browser" || n == "iframe" || n == "frame" || n == "editor";
  },
  
  cmdInspectBrowser: function()
  {
    var node = this.selectedNode;
    var n = node.localName.toLowerCase();
    if (node && n == "browser" && node.namespaceURI == kXULNSURI) {
      // xul browser
      this.subject = node.contentDocument;
    } else if (node && n == "tabbrowser" && node.namespaceURI == kXULNSURI) {
      // xul tabbrowser
      this.subject = node.contentDocument;
    } else if (n == "iframe" && node.namespaceURI == kXULNSURI) {
      // xul iframe
      this.subject = node.contentDocument;
    } else if (n == "iframe" || n == "frame") {
      // html iframe or frame
      this.subject = node.contentDocument;
    } else if (n == "editor") {
      // editor
      this.subject = node.contentDocument;
    }
  },
 
  cmdInspectInNewWindow: function()
  {
    var node = this.selectedNode;
    if (node)
      inspectObject(node);
  },
  
  ////////////////////////////////////////////////////////////////////////////
  //// XML Serialization

  cmdCopySelectedXML: function()
  {
    var node = this.selectedNode;
    if (node) {
      var xml = this.toXML(node);
    
      var helper = XPCU.getService(kClipboardHelperCID, "nsIClipboardHelper");
      helper.copyString(xml);
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
      
    if (aNode.nodeType == Node.ELEMENT_NODE) {
      line += "<" + aNode.localName;

      var attrIndent = "";
      for (i = 0; i < line.length; ++i)
        attrIndent += " ";
  
      for (i = 0; i < aNode.attributes.length; ++i) {
        var a = aNode.attributes[i];
        var attr = " " + a.localName + '="' + unicodeToEntity(a.nodeValue) + '"';
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
    } else if (aNode.nodeType == Node.TEXT_NODE) {
      s += unicodeToEntity(aNode.data);
    } else if (aNode.nodeType == Node.COMMENT_NODE) {
      s += line + "<!--" + unicodeToEntity(aNode.data) + "-->\n";
    }
    
    return s;
  },
  
  ////////////////////////////////////////////////////////////////////////////
  //// Click Selection

  selectByClick: function()
  {
    if (this.mSelecting) {
      this.stopSelectByClick();
      this.removeClickListeners();
    } else {
      // wait until after user releases the mouse after selecting this command from a UI element
      window.setTimeout("viewer.startSelectByClick()", 10);
    }
  },
  
  startSelectByClick: function()
  {
    this.mSelecting = true;
    this.mSelectDocs = this.getAllDocuments();

    for (var i = 0; i < this.mSelectDocs.length; ++i) {
      this.mSelectDocs[i].addEventListener("mousedown", MouseDownListener, true);
      this.mSelectDocs[i].addEventListener("mouseup", EventCanceller, true);
      this.mSelectDocs[i].addEventListener("click", ListenerRemover, true);
      // If use moves the mouse out of the original target area, there
      // will be no onclick event fired.... so we have to deal with
      // that.
      this.mSelectDocs[i].addEventListener("mouseout", ListenerRemover, true);
    }
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
    if (aTarget.nodeType == Node.TEXT_NODE)
      aTarget = aTarget.parentNode;
      
    this.stopSelectByClick();
    this.selectElementInTree(aTarget);
  },

  stopSelectByClick: function()
  {
    this.mSelecting = false;

    this.mPanel.panelset.setCommandAttribute("cmd:selectByClick", "checked", null);
  },

  removeClickListeners: function()
  {
    for (var i = 0; i < this.mSelectDocs.length; ++i) {
      this.mSelectDocs[i].removeEventListener("mousedown", MouseDownListener, true);
      this.mSelectDocs[i].removeEventListener("mouseup", EventCanceller, true);
      this.mSelectDocs[i].removeEventListener("click", ListenerRemover, true);
      this.mSelectDocs[i].removeEventListener("mouseout", ListenerRemover, true);
    }
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
    this.mFindWalker = this.createDOMWalker(this.mDOMView.rootNode);
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
        this.selectElementInTree(result);
        this.mDOMTree.focus();
      } else {
        var bundle = this.mPanel.panelset.stringBundle;
        var msg = bundle.getString("findNodesDocumentEnd.message");
        var title = bundle.getString("findNodesDocumentEnd.title");

        var promptService = XPCU.getService(kPromptServiceCID, "nsIPromptService");
        promptService.alert(window, title, msg);
      }
    }
  },

  doFindElementById: function(aWalker)
  {
    var re = new RegExp(this.mFindParams[0], "i");

    return aWalker.currentNode
           && "id" in aWalker.currentNode
           && re.test(aWalker.currentNode.id);
  },

  doFindElementsByTagName: function(aWalker)
  {
    var re = new RegExp(this.mFindParams[0], "i");

    return aWalker.currentNode
           && aWalker.currentNode.nodeType == Node.ELEMENT_NODE
           && re.test(aWalker.currentNode.localName);
  },

  doFindElementsByAttr: function(aWalker)
  {
    var re = new RegExp(this.mFindParams[1], "i");

    return aWalker.currentNode
           && aWalker.currentNode.nodeType == Node.ELEMENT_NODE
           && (this.mFindParams[1] == ""
              ? aWalker.currentNode.hasAttribute(this.mFindParams[0])
              : re.test(aWalker.currentNode.getAttribute(this.mFindParams[0])));
  },
  
  ///////////////////////////////////////////////////////////////////////////
  // Takes an element from the document being inspected, finds the treeitem
  // which represents it in the DOM tree and selects that treeitem.
  //
  // @param aEl - element from the document being inspected
  ///////////////////////////////////////////////////////////////////////////

  selectElementInTree: function(aEl, aExpand, aQuickie)
  {
    var bx = this.mDOMTree.treeBoxObject;

    if (!aEl) {
      bx.view.selection.select(null);
      return false;      
    }
      
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

    // we've got all the ancestors, now open them 
    // one-by-one from the top on down
    var view = bx.view;
    var lastIndex;
    for (var i = line.length-1; i >= 0; i--) {
      index = this.getRowIndexFromNode(line[i]);
      if (index < 0) 
        break; // can't find row, so stop trying to descend
      if ((aExpand || i > 0) && !view.isContainerOpen(index)) {
        view.toggleOpenState(index);
      }
      lastIndex = index;
    }

    if (!aQuickie && lastIndex >= 0) {
      view.selection.select(lastIndex);
      bx.ensureRowIsVisible(lastIndex);
    }
    
    return aQuickie;
  },
  
  ///////////////////////////////////////////////////////////////////////////
  // Remember which rows are open, and which row is selected. Then rebuild tree,
  // re-open previously opened rows, and re-select previously selected row
  ///////////////////////////////////////////////////////////////////////////
  rebuild: function()
  {
    var selNode = this.getNodeFromRowIndex(this.mDOMTree.currentIndex);
    this.mDOMView.selection.select(null);
    
    var opened = [];
    var i;
    for (i = 0; i < this.mDOMView.rowCount; ++i) {
      if (this.mDOMView.isContainerOpen(i))
        opened.push(this.getNodeFromRowIndex(i));
    }
    
    this.mDOMView.rebuild();
    
    for (i = 0; i < opened.length; ++i)
      this.selectElementInTree(opened[i], true, true);
    
    this.selectElementInTree(selNode);
  },
  
  createDOMWalker: function(aRoot)
  {
    var walker = XPCU.createInstance("@mozilla.org/inspector/deep-tree-walker;1", "inIDeepTreeWalker");
    walker.showAnonymousContent = this.mDOMView.showAnonymousContent;
    walker.showSubDocuments = this.mDOMView.showSubDocuments;
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
  },

  // XX re-implement custom columns code some-day  

  saveColumns: function()
  {
    var cols = this.mColumns.join(",");
    PrefUtils.setPref("inspector.dom.columns", cols);
  },

  ////////////////////////////////////////////////////////////////////////////
  //// Flashing

  get flasher()
  {
    if (!("mFlasher" in this)) {
      this.mFlasher = new Flasher(PrefUtils.getPref("inspector.blink.border-color"), 
                                  PrefUtils.getPref("inspector.blink.border-width"), 
                                  PrefUtils.getPref("inspector.blink.duration"), 
                                  PrefUtils.getPref("inspector.blink.speed"),
                                  PrefUtils.getPref("inspector.blink.invert"));
    }
    
    return this.mFlasher;
  },

  flashElement: function(aElement)
  {
    // make sure we only try to flash element nodes, and don't 
    // flash the documentElement (it's too darn big!)
    if (aElement.nodeType == Node.ELEMENT_NODE &&
        aElement != aElement.ownerDocument.documentElement) {
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

  toggleFlashSelected: function()
  {
    this.setFlashSelected(!this.mFlashSelected);
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
      } else if (aName == "inspector.blink.invert") {
        this.mFlasher.invert = PrefUtils.getPref("inspector.blink.invert");
      }
    }
  },

  ////////////////////////////////////////////////////////////////////////////
  //// Uncategorized
  
  getAllDocuments: function()
  {
    var doc = this.mDOMView.rootNode;
    var results = [doc];
    this.findDocuments(doc, results);
    return results;
  },
  
  findDocuments: function(aDoc, aArray)
  {
    this.addKidsToArray(aDoc.getElementsByTagName("frame"), aArray);
    this.addKidsToArray(aDoc.getElementsByTagName("iframe"), aArray);
    this.addKidsToArray(aDoc.getElementsByTagNameNS(kXULNSURI, "browser"), aArray);
    this.addKidsToArray(aDoc.getElementsByTagNameNS(kXULNSURI, "tabbrowser"), aArray);
    this.addKidsToArray(aDoc.getElementsByTagNameNS(kXULNSURI, "editor"), aArray);
  },
  
  addKidsToArray: function(aKids, aArray)
  {
    for (var i = 0; i < aKids.length; ++i) {
      try {
        aArray.push(aKids[i].contentDocument);
        // Now recurse down into the kid and look for documents there
        this.findDocuments(aKids[i].contentDocument, aArray);
      } catch (ex) {
        // if we can't access the content document, skip it
      }
    }
  },
  
  get selectedNode()
  {
    var index = this.mDOMTree.currentIndex;
    return index >= 0 ? this.getNodeFromRowIndex(index) : null;
  },

  getNodeFromRowIndex: function(aIndex)
  {
    try {
      return this.mDOMView.getNodeFromRowIndex(aIndex);
    } catch (ex) {
      return null;
    }
  },
  
  getRowIndexFromNode: function(aNode)
  {
    try {
      return this.mDOMView.getRowIndexFromNode(aNode);
    } catch (ex) {
      return -1;
    }
  },
  
  canPaste: function(aFlavour)
  {
    return aFlavour == "Inspector-DOM-Node";
  }
  
};

////////////////////////////////////////////////////////////////////////////
//// Command Objects

function cmdEditDelete() {}
cmdEditDelete.prototype =
{
  node: null,
  nextSibling: null,
  parentNode: null,
  
  doCommand: function()
  {
    var node = this.node ? this.node : viewer.selectedNode;
    if (node) {
      this.node = node;
      this.nextSibling = node.nextSibling;
      this.parentNode = node.parentNode;
      node.parentNode.removeChild(node);
    }
  },
  
  undoCommand: function()
  {
    if (this.node) {
      if (this.nextSibling)
        this.parentNode.insertBefore(this.node, this.nextSibling);
      else
        this.parentNode.appendChild(this.node);        
    }
  }
};

function cmdEditCut() {}
cmdEditCut.prototype =
{
  cmdCopy: null,
  cmdDelete: null,
  doCommand: function()
  {
    if (!this.cmdCopy) {
      this.cmdDelete = new cmdEditDelete();
      this.cmdCopy = new cmdEditCopy();
    }
    this.cmdCopy.doCommand();
    this.cmdDelete.doCommand();    
  },

  undoCommand: function()
  {
    this.cmdDelete.undoCommand();    
    this.cmdCopy.undoCommand();
  }
};

function cmdEditCopy() {}
cmdEditCopy.prototype =
{
  copiedNode: null,
  previousData: null,
  previousFlavor: null,
    
  doCommand: function()
  {
    var copiedNode = null;
    if (!this.copiedNode) {
      copiedNode = viewer.selectedNode;
      if (copiedNode) {
        this.copiedNode = copiedNode;
        this.previousData = viewer.pane.panelset.getClipboardData();
        this.previousFlavor = viewer.pane.panelset.clipboardFlavor;
      }
    } else
      copiedNode = this.copiedNode;
      
    viewer.pane.panelset.setClipboardData(copiedNode, "inspector/dom-node");
  },
  
  undoCommand: function()
  {
    viewer.pane.panelset.setClipboardData(this.previousData, this.previousFlavor);
  }
};

function cmdEditPaste() {}
cmdEditPaste.prototype =
{
  pastedNode: null,
  pastedBefore: null,
  
  doCommand: function()
  {
    var node = this.pastedNode ? this.pastedNode : viewer.pane.panelset.getClipboardData();
    var selected = this.pastedBefore ? this.pastedBefore : viewer.selectedNode;
    if (selected) {
      this.pastedNode = node.cloneNode(true);
      this.pastedBefore = selected;
      if (selected.nextSibling)
        selected.parentNode.insertBefore(this.pastedNode, selected.nextSibling);
      else
        selected.parentNode.appendChild(this.pastedNode);
      return false;
    }
    return true;
  },
  
  undoCommand: function()
  {
    if (this.pastedNode)
      this.pastedNode.parentNode.removeChild(this.pastedNode);
  }
};

////////////////////////////////////////////////////////////////////////////
//// Listener Objects

var MouseDownListener = {
  handleEvent: function(aEvent)
  {
    var target = viewer.mDOMView.showAnonymousContent ? aEvent.originalTarget : aEvent.target;
    if (aEvent.type == "mousedown") {
      aEvent.stopPropagation();
      aEvent.preventDefault();
      viewer.doSelectByClick(target);
    }
    else if (aEvent.type == "mouseover")
      viewer.selectByClickOver(target);
  }
};

var EventCanceller = {
  handleEvent: function(aEvent)
  {
    aEvent.stopPropagation();
    aEvent.preventDefault();
  }
};

var ListenerRemover = {
  handleEvent: function(aEvent)
  {
    if (!viewer.mSelecting) {
      if (aEvent.type == "click") {
        aEvent.stopPropagation();
        aEvent.preventDefault();
      }
      viewer.removeClickListeners();
    }
  }
};

var PrefChangeObserver = {
  observe: function(aSubject, aTopic, aData)
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

function unicodeToEntity(text)
{
  const charTable = {
    '&': "&amp;",
    '<': "&lt;",
    '>': "&gt;",
    '"': "&quot;"
  };

  function charTableLookup(letter) {
    return charTable[letter];
  }

  function convertEntity(letter) {
    try {
      return gEntityConverter.ConvertToEntity(letter, entityVersion);
    } catch (ex) {
      return letter;
    }
  }

  if (!gEntityConverter) {
    try {
      gEntityConverter =
        Components.classes["@mozilla.org/intl/entityconverter;1"]
                  .createInstance(Components.interfaces.nsIEntityConverter);
    } catch (ex) { }
  }

  const entityVersion = Components.interfaces.nsIEntityConverter.entityW3C;

  var str = text;

  // replace chars in our charTable
  str = str.replace(/[<>&"]/g, charTableLookup);

  // replace chars > 0x7f via nsIEntityConverter
  str = str.replace(/[^\0-\u007f]/g, convertEntity);

  return str;
}
