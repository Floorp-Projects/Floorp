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

  this.mElementCreated = false;
  if (typeof(aParentNode) === "string" && aParentNode.indexOf("create-element") >= 0)
    this.mElementTxnID = aParentNode;
  else
    this.mElementCreated = true;

  this.mParentNode  = aParentNode;
  this.mChildOffset = aChildOffset;
  
  this.mID          = "create-element::";
} 
 
vxCreateElementTxn.prototype = {
  __proto__: vxBaseTxn.prototype,

  doTransaction: function ()
  {
    if (!this.mElementCreated)
      return;
     
    var irq = { };
    this.notifyListeners("willDo", this, irq);
    this.mElement = this.mDocument.createElement(this.mLocalName);
    this.insertNode();
    this.notifyListeners("didDo", this, irq);
  },

  undoTransaction: function ()
  {
    var irq = { };
    this.notifyListeners("willUndo", this, irq);
    this.mParentNode.removeChild(this.mElement);
    this.notifyListeners("didUndo", this, irq);
  },
  
  redoTransaction: function ()
  {
    var irq = { };
    this.notifyListeners("willRedo", this, irq);
    this.insertNode();
    this.notifyListeners("didRedo", this, irq);
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
    return commandString;    
  },

  /** 
   * Implementation of nsITransactionListener
   */
  didDo: function (aTransactionManager, aTransaction, aInterrupt) 
  {
    if (aTransaction.commandString.indexOf("create-element") >= 0) {
      this.mParentNode = aTransaction.mElement;
      this.mElementCreated = true;
    }
  }
  
};

/** 
 * txnStack["create-element"]("button", someBox, 3);
 **/
 
 
 
 