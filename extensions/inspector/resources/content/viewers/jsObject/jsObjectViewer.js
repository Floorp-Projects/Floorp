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
* JSObjectViewer --------------------------------------------
*  The viewer for all facets of a javascript object.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
****************************************************************/

//////////// global variables /////////////////////

var viewer;
var bundle;

//////////// global constants ////////////////////

const kClipboardHelperCID  = "@mozilla.org/widget/clipboardhelper;1";

//////////////////////////////////////////////////

window.addEventListener("load", JSObjectViewer_initialize, false);

function JSObjectViewer_initialize()
{
  bundle = document.getElementById("inspector-bundle");
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
    var ti = this.addTreeItem(this.mTreeKids, bundle.getString("root.title"), aObject, aObject);
    ti.setAttribute("open", "true");

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
  
  isCommandEnabled: function(aCommand)
  {
    return false;
  },
  
  getCommand: function(aCommand)
  {
    return null;
  },
  
  ////////////////////////////////////////////////////////////////////////////
  //// event dispatching

  addObserver: function(aEvent, aObserver) { this.mObsMan.addObserver(aEvent, aObserver); },
  removeObserver: function(aEvent, aObserver) { this.mObsMan.removeObserver(aEvent, aObserver); },

  ////////////////////////////////////////////////////////////////////////////
  //// UI Commands

  cmdCopyValue: function()
  {
    var sel = getSelectedItem();
    if (sel) {
      var val = sel.__JSValue__;
      if (val) {
        var helper = XPCU.getService(kClipboardHelperCID, "nsIClipboardHelper");
        helper.copyString(val);
      }
    }
  },
  
  cmdEvalExpr: function()
  {
    var sel = getSelectedItem();
    if (sel) {
      var win = openDialog("chrome://inspector/content/viewers/jsObject/evalExprDialog.xul", 
                           "_blank", "chrome", this, sel);
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
    var sel = getSelectedItem();
    if (sel)
      inspectObject(sel.__JSValue__);
  },
  
  ////////////////////////////////////////////////////////////////////////////
  //// tree construction

  emptyTree: function(aTreeKids)
  {
    while (aTreeKids.hasChildNodes()) {
      aTreeKids.removeChild(aTreeKids.lastChild);
    }
  },
  
  buildPropertyTree: function(aTreeChildren, aObject)
  {
    for (var prop in aObject) {
      try {
        this.addTreeItem(aTreeChildren, prop, aObject[prop], aObject);
      } catch (ex) {
        // hide unsightly NOT YET IMPLEMENTED errors when accessing certain properties
      }
    }
  },
  
  addTreeItem: function(aTreeChildren, aName, aValue, aObject)
  {
    var ti = document.createElement("treeitem");
    ti.__JSObject__ = aObject;
    ti.__JSValue__ = aValue;
    
    var value;
    if (aValue === null) {
      value = "(null)";
    } else if (aValue === undefined) {
      value = "(undefined)";
    } else {
      try {
        value = aValue.toString();
        value = value.replace(/\n|\r|\t|\v/g, " ");
      } catch (ex) {
        value = "";
      }
    }
    
    ti.setAttribute("typeOf", typeof(aValue));

    if (typeof(aValue) == "object" && aValue !== null) {
      ti.setAttribute("container", "true");
    } else if (typeof(aValue) == "string")
      value = "\"" + value + "\"";
    
    var tr = document.createElement("treerow");
    ti.appendChild(tr);
    
    var tc = document.createElement("treecell");
    tc.setAttribute("label", aName);
    tr.appendChild(tc);
    tc = document.createElement("treecell");
    tc.setAttribute("label", value);
    if (aValue === null) {
      tc.setAttribute("class", "inspector-null-value-treecell");
    }
    if (aName == "nodeType")
      tc.setAttribute("tooltiptext", nodeTypeToText(aValue));
    tr.appendChild(tc);
    
    aTreeChildren.appendChild(ti);

    // listen for changes to open attribute
    this.mTreeKids.addEventListener("DOMAttrModified", onTreeItemAttrModified, false);
    
    return ti;
  },
  
  openTreeItem: function(aItem)
  {
    var treechildren = aItem.getElementsByTagName("treechildren").item(0);
    if (!treechildren) {
      treechildren = document.createElement("treechildren");
      this.buildPropertyTree(treechildren, aItem.__JSValue__);
      aItem.appendChild(treechildren);
    }
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

function getSelectedItem()
{
  var tree = document.getElementById("treeJSObject");
  if (tree.view.selection.count)
    return tree.contentView.getItemAtIndex(tree.currentIndex);
  else 
    return null;    
}

function toggleItem(aItem)
{
  var tree = document.getElementById("treeJSObject");
  var row = tree.currentView.getIndexOfItem(aItem);
  if (row >= 0) {
    tree.view.toggleOpenState(row);
  }
}
