/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 */

/***************************************************************
* DOMNodeViewer --------------------------------------------
*  The default viewer for DOM Nodes
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/util.js
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
*   chrome://inspector/content/jsutil/rdf/RDFU.js
****************************************************************/

//////////// global variables /////////////////////

var viewer;

//////////// global constants ////////////////////

const kDOMDataSourceIID    = "@mozilla.org/rdf/datasource;1?name=Inspector_DOM";

//////////////////////////////////////////////////

window.addEventListener("unload", DOMNodeViewer_destroy, false);
window.addEventListener("load", DOMNodeViewer_initialize, false);

function DOMNodeViewer_initialize()
{
  viewer = new DOMNodeViewer();
  viewer.initialize(parent.FrameExchange.receiveData(window));
}

function DOMNodeViewer_destroy()
{
  viewer.destroy();
}

////////////////////////////////////////////////////////////////////////////
//// class DOMNodeViewer 

function DOMNodeViewer()  // implements inIViewer
{
  this.mObsMan = new ObserverManager(this);

  this.mURL = window.location;
  this.mAttrTree = document.getElementById("trAttributes");

  var ds = XPCU.createInstance(kDOMDataSourceIID, "inIDOMDataSource");
  this.mDS = ds;
  ds.addFilterByType(2, true);
  this.mAttrTree.database.AddDataSource(ds);
}

DOMNodeViewer.prototype = 
{
  ////////////////////////////////////////////////////////////////////////////
  //// Initialization
  
  mDS: null,
  mSubject: null,
  mPane: null,

  get selectedAttribute() { return this.mAttrTree.selectedItems[0] },

  ////////////////////////////////////////////////////////////////////////////
  //// interface inIViewer

  //// attributes 

  get uid() { return "domNode" },
  get pane() { return this.mPane },

  get selection() { return null },

  get subject() { return this.mSubject },
  set subject(aObject) 
  {
    this.mSubject = aObject;
    var deck = document.getElementById("dkContent");
    
    if (aObject.nodeType == 1) {
      deck.setAttribute("index", 0);
      
      if (aObject.ownerDocument != this.mDS.document)
        this.mDS.document = aObject.ownerDocument;
      
      this.setTextValue("nodeName", aObject.nodeName);
      this.setTextValue("nodeType", aObject.nodeType);
      this.setTextValue("nodeValue", aObject.nodeValue);
      this.setTextValue("namespace", aObject.namespaceURI);
  
      var res = this.mDS.getResourceForObject(aObject);
      try {
        this.mAttrTree.setAttribute("ref", res.Value);
      } catch (ex) {
        debug("ERROR: While rebuilding attribute tree\n" + ex);
      }
    } else {
      deck.setAttribute("index", 1);
      var txb = document.getElementById("txbTextNodeValue");
      txb.value = aObject.nodeValue;
    }
        
    this.mObsMan.dispatchEvent("subjectChange", { subject: aObject });
  },

  // methods

  initialize: function(aPane)
  {
    this.mPane = aPane;
    aPane.notifyViewerReady(this);
  },

  destroy: function()
  {
  },

  ////////////////////////////////////////////////////////////////////////////
  //// event dispatching

  addObserver: function(aEvent, aObserver) { this.mObsMan.addObserver(aEvent, aObserver); },
  removeObserver: function(aEvent, aObserver) { this.mObsMan.removeObserver(aEvent, aObserver); },

  ////////////////////////////////////////////////////////////////////////////
  //// UI Commands

  cmdNewAttribute: function()
  {
    var name = prompt("Enter the attribute name:", "");
    if (name) {
      var attr = this.mSubject.getAttributeNode(name);
      if (!attr)
        this.mSubject.setAttribute(name, "");
      var res = this.mDS.getResourceForObject(attr);
      var kids = this.mAttrTree.getElementsByTagName("treechildren")[1];
      var item = kids.childNodes[kids.childNodes.length-1];
      if (item) {
        var cell = item.getElementsByAttribute("ins-type", "value")[0];
        this.mAttrTree.selectItem(item);
        this.mAttrTree.startEdit(cell);
      }
    }
  },
  
  cmdEditSelectedAttribute: function()
  {
    var item = this.mAttrTree.selectedItems[0];
    var cell = item.getElementsByAttribute("ins-type", "value")[0];
    this.mAttrTree.startEdit(cell);
  },

  cmdDeleteSelectedAttribute: function()
  {
    var item = this.selectedAttribute;
    if (item) {
      var attrname = InsUtil.getDSProperty(this.mDS, item.id, "nodeName");
      this.mSubject.removeAttribute(attrname);
    }
  },

  cmdSetSelectedAttributeValue: function(aItem, aValue)
  {
    if (aItem) {
      var attrname = InsUtil.getDSProperty(this.mDS, aItem.id, "nodeName");
      this.mSubject.setAttribute(attrname, aValue);
    }
  },

  ////////////////////////////////////////////////////////////////////////////
  //// Uncategorized

  setTextValue: function(aName, aText)
  {
    var field = document.getElementById("tx_"+aName);
    if (field) {
      field.setAttribute("value", aText);
    }
  }

};

