/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/***************************************************************
* JSObjectViewer --------------------------------------------
*  The viewer for all facets of a javascript object.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
****************************************************************/

//////////// global variables /////////////////////

var viewer;

//////////// global constants ////////////////////

const kClipboardHelperCID  = "@mozilla.org/widget/clipboardhelper;1";
const kGlobalClipboard     = Components.interfaces.nsIClipboard.kGlobalClipboard;

//////////////////////////////////////////////////

window.addEventListener("load", JSObjectViewer_initialize, false);

function JSObjectViewer_initialize()
{
  viewer = new JSObjectViewer();
  viewer.initialize(parent.FrameExchange.receiveData(window));
}

////////////////////////////////////////////////////////////////////////////
//// class JSObjectViewer

function JSObjectViewer()
{
  this.mObsMan = new ObserverManager(this);
}

JSObjectViewer.prototype = 
{
  ////////////////////////////////////////////////////////////////////////////
  //// Initialization
  
  mSubject: null,
  mPane: null,

  ////////////////////////////////////////////////////////////////////////////
  //// interface inIViewer

  get uid() { return "jsObject" },
  get pane() { return this.mPane },

  get selection() { return this.mSelection },
  
  get subject() { return this.mSubject },
  set subject(aObject) 
  {
    this.mSubject = aObject;
    this.emptyTree(this.mTreeKids);
    var ti = this.addTreeItem(this.mTreeKids, "target", aObject, aObject);
    this.openTreeItem(ti);
    window.setTimeout(function(aItem) { aItem.toggleOpenState() }, 1, ti);

    this.mObsMan.dispatchEvent("subjectChange", { subject: aObject });
  },

  initialize: function(aPane)
  {
    this.mPane = aPane;
    this.mTree = document.getElementById("treeJSObject");
    this.mTreeKids = document.getElementById("trchJSObject");
    
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

  cmdCopyValue: function()
  {
    var sels = this.mTree.selectedItems;
    if (sels.length > 0) {
      var val = sels[0].__JSValue__;
      if (val) {
        var helper = XPCU.getService(kClipboardHelperCID, "nsIClipboardHelper");
        helper.copyStringToClipboard(val, kGlobalClipboard);    
      }
    }
  },
  
  cmdEvalExpr: function()
  {
    var sels = this.mTree.selectedItems;
    if (sels.length > 0) {
      var win = openDialog("chrome://inspector/content/viewers/jsObject/evalExprDialog.xul", 
                           "_blank", "chrome", this, sels[0]);
    }
  },  
  
  doEvalExpr: function(aExpr, aItem, aNewView)
  {
    // TODO: I should really write some C++ code to execute the 
    // js code in the js context of the inspected window
    
    try {
      var f = Function("target", aExpr);
      var result = f(aItem.__JSValue__);
      
      if (result) {
        if (aNewView) {
          inspectObject(result);
        } else {
          this.subject = result;
        }
      }
    } catch (ex) {
      dump("Error in expression.\n");
      throw (ex);
    }
  },  
  
  cmdInspectInNewView: function()
  {
    var sel = this.mTree.selectedItems[0];
    inspectObject(sel.__JSValue__);
  },
  
  ////////////////////////////////////////////////////////////////////////////
  //// tree construction

  emptyTree: function(aTreeKids)
  {
    var kids = aTreeKids.childNodes;
    for (var i = 0; i < kids.length; ++i) {
      aTreeKids.removeChild(kids[i]);
    }
  },
  
  buildPropertyTree: function(aTreeChildren, aObject)
  {
    try {
      for (var prop in aObject)
        this.addTreeItem(aTreeChildren, prop, aObject[prop], aObject);
    } catch (ex) {
      // hide unsightly NOT YET IMPLEMENTED errors when accessing certain properties
    }
  },
  
  addTreeItem: function(aTreeChildren, aName, aValue, aObject)
  {
    var ti = document.createElement("treeitem");
    ti.__JSObject__ = aObject;
    ti.__JSValue__ = aValue;
    
    var value = aValue.toString();
    value = value.replace(/\n|\r|\t/, " ");
    
    ti.setAttribute("typeOf", typeof(aValue));

    if (typeof(aValue) == "object") {
      ti.setAttribute("container", "true");
    } else if (typeof(aValue) == "string")
      value = "\"" + value + "\"";
    
    var tr = document.createElement("treerow");
    ti.appendChild(tr);
    
    var tc = document.createElement("treecell");
    tc.setAttribute("class", "treecell-indent");
    tc.setAttribute("label", aName);
    tr.appendChild(tc);
    tc = document.createElement("treecell");
    tc.setAttribute("label", value);
    tr.appendChild(tc);
    
    aTreeChildren.appendChild(ti);

    // listen for changes to open attribute
    this.mTreeKids.addEventListener("DOMAttrModified", onTreeItemAttrModified, false);
    
    return ti;
  },
  
  openTreeItem: function(aItem)
  {
    var kids = aItem.getElementsByTagName("treechildren");
    if (kids.length == 0) {
      kids = document.createElement("treechildren");
      aItem.appendChild(kids);
    }
    
    this.buildPropertyTree(kids, aItem.__JSValue__);
  },
  
  onCreateContext: function(aPopup)
  {
  }
  
};

function onTreeItemAttrModified(aEvent)
{
  if (aEvent.attrName == "open")
    viewer.openTreeItem(aEvent.target);
}
