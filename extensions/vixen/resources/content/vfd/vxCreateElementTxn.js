/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Ben Goodger <ben@netscape.com> (Original Author)
 */

/** 
 * vxCreateElementTxn - Transaction for creating DOM elements
 * 
 * e.g., create a new element in the VFD document using the palette.
 */

function vxCreateElementTxn(aDocument, aLocalName, aParentNode, aChildOffset)
{
  this.mDocument    = aDocument;
  this.mLocalName   = aLocalName;
  this.mParentNode  = aParentNode;
  this.mChildOffset = aChildOffset;
  this.mElement     = this.mDocument.createElement(this.mLocalName);
} 
 
vxCreateElementTxn.prototype = {
  doTransaction: function ()
  {
    this.insertNode();
  },

  undoTransaction: function ()
  {
    _ddf("element", this.mElement.localName);
    _ddf("parent", this.mParentNode.localName);
    for (var i = 0; i < this.mParentNode.childNodes.length; i++) {
      _ddf("childnode at " + i, this.mParentNode.childNodes[i]);
    }
    this.mParentNode.removeChild(this.mElement);
  },
  
  redoTransaction: function ()
  {
    this.insertNode();
  },

  insertNode: function () 
  {
    if (this.mParentNode.hasChildNodes() &&
        this.mParentNode.childNodes.length < this.mChildOffset+1) {
      var nextSibling = this.mParentNode.childNodes[this.mChildOffset+1]
      this.mParentNode.insertBefore(this.mElement, nextSibling);
    }
    else {
      this.mParentNode.appendChild(this.mElement);
    }
  },
  
  get commandString()
  {
    var commandString = "create-element";
    commandString = this.mRemoveFlag ? "remove," : "set,";
    commandString += this.mElement.id + ",";
    commandString += this.mAttribute + ",";
    commandString += this.mValue;
    return commandString;    
  }
};

/** 
 * txnStack["create-element"]("button", someBox, 3);
 **/
 
 
 
 