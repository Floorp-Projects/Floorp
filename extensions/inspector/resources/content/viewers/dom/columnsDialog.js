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
* ColumnsDialog --------------------------------------------
*  Dialog box for editing the columns in the DOM Viewer tree.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
****************************************************************/

//////////// global variables /////////////////////

var dialog;

var gColumnTitles = [
  "nodeName",
  "nodeValue",
  "nodeType",
  "prefix",
  "localName",
  "namespaceURI"
];

//////////// global constants ////////////////////

//////////////////////////////////////////////////

window.addEventListener("load", ColumnsDialog_initialize, false);
window.addEventListener("unload", ColumnsDialog_destroy, false);

function ColumnsDialog_initialize()
{
  dialog = new ColumnsDialog();
  dialog.initialize();
}

function ColumnsDialog_destroy()
{
  dialog.destroy();
}

////////////////////////////////////////////////////////////////////////////
//// class ColumnsDialog

function ColumnsDialog()
{
  this.mViewer = window.arguments[0];
  this.mColBoxHash = {};
}

ColumnsDialog.prototype = 
{
  mBox: null,
  mDraggingBox: null,
  
  get box() { return this.mBox },
  
  initialize: function()
  {
    // bug 56270 - dragSession.sourceDocument is null --
    // causes me to code this very temporary, very nasty hack
    // to make sure I get notified when a column is dropped
    opener.viewer.mDOMTree.onClientDrop = ColumnsDialogDragDropOut;
    
    this.buildContents();
    
    // notify the dom viewer that we've opened
    this.mViewer.onColumnsDialogReady(this);
  },
  
  destroy: function()
  {
    // notify the dom viewer that we're going away
    this.mViewer.onColumnsDialogClose(this);
  },
  
  buildContents: function()
  {
    var box = document.getElementById("bxColumns");
    this.mBox = box;
    
    // create the special attribute box
    var item = this.createAttrItem();
    box.appendChild(item);
    
    // add all boxes except those that are already
    // in the viewer
    for (var i = 0; i < gColumnTitles.length; ++i)
      if (!this.mViewer.hasColumn(gColumnTitles[i])) {
        this.addItem(gColumnTitles[i]);
      }  
  },
  
  addItem: function(aValue)
  {
    if (!aValue || aValue.charAt(0) == "@") 
      return null;
      
    item = this.createItem(aValue);
    this.mColBoxHash[aValue] = item;
    if (item) {
      this.mBox.appendChild(item);
      window.sizeToContent();
    }
  },
  
  removeItem: function(aValue)
  {
    if (!aValue || aValue.charAt(0) == "@")
      return null;
      
    var colBox = this.mColBoxHash[aValue];
    if (!colBox._isAttrCol) {
      this.mBox.removeChild(colBox);
      window.sizeToContent();
    }
  },
  
  createItem: function(aValue)
  {
    var text = document.createElementNS(kXULNSURI, "text");
    text._ColValue = aValue;
    text._isColBox = true;
    text._isAttrCol = false;
    text.setAttribute("class", "column-selector");
    text.setAttribute("value", aValue);

    return text;
  },
  
  createAttrItem: function(aValue)
  {
    var box = document.createElementNS(kXULNSURI, "box");
    box._isColBox = true;
    box._isAttrCol = true;
    box.setAttribute("class", "column-selector");
    box.setAttribute("align", "center");

    var text = document.createElementNS(kXULNSURI, "text");
    text.setAttribute("value", "Attr");
    box.appendChild(text);

    var txf = document.createElementNS(kXULNSURI, "textbox");
    txf.setAttribute("class", "attr-column-selector");
    txf.setAttribute("flex", 1);
    box.appendChild(txf);

    return box;
  },
    
  ////////////////////////////////////////////////////////////////////////////
  //// Drag and Drop

  onDragGesture: function(aEvent)
  {
    var box = this.getBoxTarget(aEvent.target);
    this.setDraggingBox(box);
    if (!box) return false;
    
    var column = this.getColumnValue(box);
    if (!column) return false;
    
    DNDUtils.invokeSession(aEvent.target, ["TreeBuilder/column-add"], [column]);
    
    return false;
  },
  
  onDragDropOut: function()
  {
    var box = document.getElementById("bxColumns");
    var value =  this.mDraggingBox._ColValue;
    this.setDraggingBox(null);
    this.removeItem(value);
  },
  
  onDragDropIn: function(aEvent)
  {
    var data = DNDUtils.getData("TreeBuilder/column-remove", 0);
    var string = XPCU.QI(data, "nsISupportsString");
    this.addItem(string.data);
  },

  setDraggingBox: function(aBox)
  {
    if (this.mDraggingBox) {
      this.mDraggingBox.removeAttribute("col-dragging");
    }

    this.mDraggingBox = aBox;

    if (aBox)
      aBox.setAttribute("col-dragging", "true");
  },
  
  getBoxTarget: function(aNode)
  {
    var node = aNode;
    while (node && !node._isColBox)
      node = node.parentNode;
    return node;
  },
  
  getColumnValue: function(aColBox)
  {
    if (aColBox._isAttrCol) {
      var txf = aColBox.getElementsByTagName("textbox")[0];
      return "@" + txf.value;
    } else {
      return aColBox._ColValue;
    }
  }

};

function ColumnsDialogDragDropOut()
{
  dialog.onDragDropOut();
}
