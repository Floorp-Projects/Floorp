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
 * vxChangeAttributeTxn - Transaction for changing Attributes
 * 
 * e.g., defocus or confirmation keystroke on field in attribute/property inspector,
 *       var caTxn = vxChangeAttributeTxn(vxVFDDoc.getElementById(..), 
 *                                        ...value, 
 *                                        field.value, 
 *                                        false);
 *       txMgr.doTransaction(caTxn);  // txMgr is our own wrapper to 
 *                                    // nsITransactionManager that keeps 
 *                                    // UI elements in sync with the txMgr's deques.
 *
 * An example of attribute removal might be a selection of the attribute line, hitting
 * the delete key, or selecting the line, and choosing delete via a context menu.
 */

function vxChangeAttributeTxn(aElement, aAttribute, aValue, aRemoveFlag)
{
  this.mElement     = aElement;
  this.mAttribute   = aAttribute;
  this.mValue       = aValue;
  this.mRemoveFlag  = aRemoveFlag;
} 
 
vxChangeAttributeTxn.prototype = {
  doTransaction: function ()
  {
    this.mUndoValue = this.mElement.getAttribute(aAttribute);
    if (!this.mRemoveFlag)
      this.mElement.setAttribute(this.mAttribute, this.mValue);
    else
      this.mElement.removeAttribute(this.mAttribute);
  },

  undoTransaction: function ()
  {
    if (this.mUndoValue) 
      this.mElement.setAttribute(this.mAttribute, this.mUndoValue);
    else
      this.mElement.removeAttribute(this.mAttribute);
  },
  
  redoTransaction: function ()
  {
    if (!this.mRemoveFlag)
      this.mElement.setAttribute(this.mAttribute, this.mValue);
    else
      this.mElement.removeAttribute(this.mAttribute);
  },
  
  get commandString()
  {
    var commandString = "change-attribute,";
    commandString += this.mRemoveFlag ? "remove," : "set,";
    commandString += this.mElement.id + ",";
    commandString += this.mAttribute + ",";
    commandString += this.mValue;
    return commandString;    
  }
};

