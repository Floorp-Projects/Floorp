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
 *   Jason Barnabe <jason_barnabe@fastmail.fm>
 *   Shawn Wilsher <me@shawnwilsher.com>
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
* DOMNodeViewer --------------------------------------------
*  The default viewer for DOM Nodes
****************************************************************/

//////////// global variables /////////////////////

var viewer;

var gPromptService;

//////////// global constants ////////////////////

const kDOMViewCID          = "@mozilla.org/inspector/dom-view;1";
const kPromptServiceCID    = "@mozilla.org/embedcomp/prompt-service;1";

//////////////////////////////////////////////////

window.addEventListener("load", DOMNodeViewer_initialize, false);

function DOMNodeViewer_initialize()
{
  viewer = new DOMNodeViewer();
  viewer.initialize(parent.FrameExchange.receiveData(window));
}

////////////////////////////////////////////////////////////////////////////
//// class DOMNodeViewer 

function DOMNodeViewer()  // implements inIViewer
{
  this.mObsMan = new ObserverManager(this);

  this.mURL = window.location;
  this.mAttrTree = document.getElementById("olAttr");

  // prepare and attach the DOM DataSource
  this.mDOMView = XPCU.createInstance(kDOMViewCID, "inIDOMView");
  this.mDOMView.whatToShow = NodeFilter.SHOW_ATTRIBUTE;
  this.mAttrTree.treeBoxObject.view = this.mDOMView;
}

DOMNodeViewer.prototype = 
{
  ////////////////////////////////////////////////////////////////////////////
  //// Initialization
  
  mDOMView: null,
  mSubject: null,
  mPanel: null,

  get selectedIndex()
  {
    return this.mAttrTree.currentIndex;
  },

 /**
  * Returns an array of the selected indices
  * @return an array of indices
  */
  get selectedIndices()
  {
    var indices = [];
    var rangeCount = this.mAttrTree.view.selection.getRangeCount();
    for (var i = 0; i < rangeCount; ++i) {
      var start = {};
      var end = {};
      this.mAttrTree.view.selection.getRangeAt(i, start, end);
      for (var c = start.value; c <= end.value; ++c) {
        indices.push(c);
      }
    }
    return indices;
  },

 /**
  * Returns a DOMAttribute from the selected index
  * @return a DomAttribute
  */
  get selectedAttribute()
  {
    var index = this.selectedIndex;
    return index >= 0 ?
      new DOMAttribute(this.mDOMView.getNodeFromRowIndex(index)) : null;
  },

 /**
  * Returns an array of DOMAttributes from the selected indices
  * @return an array of DOMAttributes
  */
  get selectedAttributes()
  {
    var indices = this.selectedIndices;
    var attrs = [];
    for (var i = 0; i < indices.length; ++i) {
      attrs.push(new DOMAttribute(this.mDOMView.getNodeFromRowIndex(indices[i])));
    }
    return attrs;
  },

  ////////////////////////////////////////////////////////////////////////////
  //// interface inIViewer

  //// attributes 

  get uid() { return "domNode" },
  get pane() { return this.mPanel },

  get selection() { return null },

  get subject() { return this.mSubject },
  set subject(aObject) 
  {
    // the node value's textbox won't fire onchange when we change subjects, so 
    // let's fire it. this won't do anything if it wasn't actually changed
    viewer.pane.panelset.execCommand('cmdEditNodeValue');

    this.mSubject = aObject;
    var deck = document.getElementById("dkContent");

    switch (aObject.nodeType) {
      // things with useful nodeValues
      case Node.TEXT_NODE:
      case Node.CDATA_SECTION_NODE:
      case Node.COMMENT_NODE:
      case Node.PROCESSING_INSTRUCTION_NODE:
        deck.setAttribute("selectedIndex", 1);
        var txb = document.getElementById("txbTextNodeValue").value = 
                  aObject.nodeValue;
        break;
      //XXX this view is designed for elements, write a more useful one for
      // document nodes, etc.
      default:
        deck.setAttribute("selectedIndex", 0);
        
        this.setTextValue("nodeName", aObject.nodeName);
        this.setTextValue("nodeType", aObject.nodeType);
        this.setTextValue("namespace", aObject.namespaceURI);

        if (aObject != this.mDOMView.rootNode) {
          this.mDOMView.rootNode = aObject;
          this.mAttrTree.view.selection.select(-1);
        }
    }
    
    this.mObsMan.dispatchEvent("subjectChange", { subject: aObject });
  },

  // methods

  initialize: function(aPane)
  {
    this.mPanel = aPane;
    aPane.notifyViewerReady(this);
  },

  destroy: function()
  {
    // the node value's textbox won't fire onchange when we change views, so 
    // let's fire it. this won't do anything if it wasn't actually changed
    viewer.pane.panelset.execCommand('cmdEditNodeValue');
  },

  isCommandEnabled: function(aCommand)
  {
    switch (aCommand) {
      case "cmdEditPaste":
        var flavor = this.mPanel.panelset.clipboardFlavor;
        return (flavor == "inspector/dom-attribute" ||
                flavor == "inspector/dom-attributes");
      case "cmdEditInsert":
        return true;
      case "cmdEditCut":
      case "cmdEditCopy":
      case "cmdEditDelete":
        return this.selectedAttribute != null;
      case "cmdEditEdit":
        return this.mAttrTree.view.selection.count == 1;
      case "cmdEditNodeValue":
        // this function can be fired before the subject is set
        if (this.subject) {
          // something with a useful nodeValue
          if (this.subject.nodeType == Node.TEXT_NODE ||
              this.subject.nodeType == Node.CDATA_SECTION_NODE ||
              this.subject.nodeType == Node.COMMENT_NODE ||
              this.subject.nodeType == Node.PROCESSING_INSTRUCTION_NODE) {
            // did something change?
            return this.subject.nodeValue != 
                   document.getElementById("txbTextNodeValue").value;
          }
        }
        return false;
    }
    return false;
  },
  
  getCommand: function(aCommand)
  {
    switch (aCommand) {
      case "cmdEditCut":
        return new cmdEditCut();
      case "cmdEditCopy":
        return new cmdEditCopy(this.selectedAttributes);
      case "cmdEditPaste":
        return new cmdEditPaste();
      case "cmdEditInsert":
        return new cmdEditInsert();
      case "cmdEditEdit":
        return new cmdEditEdit();
      case "cmdEditDelete":
        return new cmdEditDelete();
      case "cmdEditNodeValue":
        return new cmdEditNodeValue();
    }
    return null;
  },
  
  ////////////////////////////////////////////////////////////////////////////
  //// event dispatching

  addObserver: function(aEvent, aObserver) { this.mObsMan.addObserver(aEvent, aObserver); },
  removeObserver: function(aEvent, aObserver) { this.mObsMan.removeObserver(aEvent, aObserver); },

  ////////////////////////////////////////////////////////////////////////////
  //// Uncategorized

  setTextValue: function(aName, aText)
  {
    var field = document.getElementById("tx_"+aName);
    if (field) {
      if (aName == "nodeType")
        field.setAttribute("tooltiptext", nodeTypeToText(aText));
      field.value = aText;
    }
  }
};

////////////////////////////////////////////////////////////////////////////
//// Command Objects

function cmdEditCut() {}
cmdEditCut.prototype =
{
  cmdCopy: null,
  cmdDelete: null,
  doCommand: function()
  {
    if (!this.cmdCopy) {
      this.cmdDelete = new cmdEditDelete();
      this.cmdCopy = new cmdEditCopy(viewer.selectedAttributes);
    }
    this.cmdCopy.doTransaction();
    this.cmdDelete.doCommand();    
  },

  undoCommand: function()
  {
    this.cmdDelete.undoCommand();    
  }
};

function cmdEditPaste() {}
cmdEditPaste.prototype =
{
  pastedAttr: null,
  previousAttrValue: null,
  subject: null,
  flavor: null,
  
  doCommand: function()
  {
    var subject, pastedAttr, flavor;
    if (this.subject) {
      subject = this.subject;
      pastedAttr = this.pastedAttr;
      flavor = this.flavor;
    } else {
      subject = viewer.subject;
      pastedAttr = viewer.pane.panelset.getClipboardData();
      flavor = viewer.pane.panelset.clipboardFlavor;
      this.pastedAttr = pastedAttr;
      this.subject = subject;
      this.flavor = flavor;
      if (flavor == "inspector/dom-attributes") {
        this.previousAttrValue = [];
        for (var i = 0; i < pastedAttr.length; ++i) {
          this.previousAttrValue[pastedAttr[i].node.nodeName] =
            viewer.subject.getAttribute(pastedAttr[i].node.nodeName);
        }
      } else if (flavor == "inspector/dom-attribute") {
        this.previousAttrValue =
          viewer.subject.getAttribute(pastedAttr.node.nodeName);
      }
    }
    
    if (subject && pastedAttr) {
      if (flavor == "inspector/dom-attributes") {
        for (var i = 0; i < pastedAttr.length; ++i) {
          subject.setAttribute(pastedAttr[i].node.nodeName,
                               pastedAttr[i].node.nodeValue);
        }
      } else if (flavor == "inspector/dom-attribute") {
        subject.setAttribute(pastedAttr.node.nodeName,
                             pastedAttr.node.nodeValue);
      }
    }
  },
  
  undoCommand: function()
  {
    if (this.pastedAttr) {
      if (this.flavor == "inspector/dom-attributes") {
        for (var i = 0; i < this.pastedAttr.length; ++i) {
          if (this.previousAttrValue[this.pastedAttr[i].node.nodeName])
            this.subject.setAttribute(this.pastedAttr[i].node.nodeName,
                      this.previousAttrValue[this.pastedAttr[i].node.nodeName]);
          else
            this.subject.removeAttribute(this.pastedAttr[i].node.nodeName);
        }
      } else if (this.flavor == "inspector/dom-attribute") {
        if (this.previousAttrValue)
          this.subject.setAttribute(this.pastedAttr.node.nodeName,
                                    this.previousAttrValue);
        else
          this.subject.removeAttribute(this.pastedAttr.node.nodeName);
      }
    }
  }
};

function cmdEditInsert() {}
cmdEditInsert.prototype =
{
  attr: null,
  subject: null,
  
  promptFor: function()
  {
    if (!gPromptService) {
      gPromptService = XPCU.getService(kPromptServiceCID, "nsIPromptService");
    }

    var attrName = { value: "" };
    var attrValue = { value: "" };
    var dummy = { value: false };

    var bundle = viewer.pane.panelset.stringBundle;
    var msg = bundle.getString("enterAttrName.message");
    var title = bundle.getString("newAttribute.title");

    if (gPromptService.prompt(window, title, msg, attrName, null, dummy)) {
      msg = bundle.getString("enterAttrValue.message");
      if (gPromptService.prompt(window, title, msg, attrValue, null, dummy)) {
        this.subject = viewer.subject;
        this.subject.setAttribute(attrName.value, attrValue.value);
        this.attr = this.subject.getAttributeNode(attrName.value);
        return false;
      }
    }
    return true;
  },
  
  doCommand: function()
  {
    if (this.attr)
      this.subject.setAttribute(this.attr.nodeName, this.attr.nodeValue);
    else
      return this.promptFor();
    return false;
  },
  
  undoCommand: function()
  {
    if (this.attr && this.subject == viewer.subject)
      this.subject.removeAttribute(this.attr.nodeName, this.attr.nodeValue);
  }
};

function cmdEditDelete() {}
cmdEditDelete.prototype =
{
  attrs: null,
  subject: null,
  
  doCommand: function()
  {
    var attrs = this.attrs ? this.attrs : viewer.selectedAttributes;
    if (attrs) {
      this.attrs = attrs;
      this.subject = viewer.subject;
      for (var i = 0; i < this.attrs.length; ++i) {
        this.subject.removeAttribute(this.attrs[i].node.nodeName);
      }
    }
  },
  
  undoCommand: function()
  {
    if (this.attrs) {
      for (var i = 0; i < this.attrs.length; ++i) {
        this.subject.setAttribute(this.attrs[i].node.nodeName,
                                  this.attrs[i].node.nodeValue);
      }
    }
  }
};

// XXX when editing the a attribute in this document:
// data:text/xml,<x a="hi&#x0a;lo&#x0d;go"/>
// You only get "hi" and not the mutltiline text (windows)
// This seems to work on Linux, but not very usable
function cmdEditEdit() {}
cmdEditEdit.prototype =
{
  attr: null,
  previousValue: null,
  newValue: null,
  subject: null,
  
  promptFor: function()
  {
    var attr = viewer.selectedAttribute.node;
    if (attr) {
      if (!gPromptService) {
        gPromptService = XPCU.getService(kPromptServiceCID, "nsIPromptService");
      }

      var attrValue = { value: attr.nodeValue };
      var dummy = { value: false };

      var bundle = viewer.pane.panelset.stringBundle;
      var msg = bundle.getString("enterAttrValue.message");
      var title = bundle.getString("editAttribute.title");

      if (gPromptService.prompt(window, title, msg, attrValue, null, dummy)) {
        this.subject = viewer.subject;
        this.newValue = attrValue.value;
        this.previousValue = attr.nodeValue;
        this.subject.setAttribute(attr.nodeName, attrValue.value);
        this.attr = this.subject.getAttributeNode(attr.nodeName);
        return false;
      }
    }
    return true;
  },
  
  doCommand: function()
  {
    if (this.attr)
      this.subject.setAttribute(this.attr.nodeName, this.newValue);
    else
      return this.promptFor();
    return false;
  },
  
  undoCommand: function()
  {
    if (this.attr)
      this.subject.setAttribute(this.attr.nodeName, this.previousValue);
  }
};

/**
 * Handles editing of node values.
 */
function cmdEditNodeValue() {
  this.newValue = document.getElementById("txbTextNodeValue").value;
  this.subject = viewer.subject;
  this.previousValue = this.subject.nodeValue;
}
cmdEditNodeValue.prototype =
{
  // remove this line for bug 179621, Phase Three
  txnType: "standard",
  
  // required for nsITransaction
  QueryInterface: txnQueryInterface,
  merge: txnMerge,
  isTransient: false,

  doTransaction: function doTransaction()
  {
    this.subject.nodeValue = this.newValue;
  },
  
  undoTransaction: function undoTransaction()
  {
    this.subject.nodeValue = this.previousValue;
    this.refreshView();
  },

  redoTransaction: function redoTransaction()
  {
    this.doTransaction();
    this.refreshView();
  },

  refreshView: function refreshView() {
    // if we're still on the same subject, update the textbox
    if (viewer.subject == this.subject) {
      document.getElementById("txbTextNodeValue").value =
               this.subject.nodeValue;
    }
  }
};
