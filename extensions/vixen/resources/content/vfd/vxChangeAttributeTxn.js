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
  this.mAttributes  = aAttribute.splice ? aAttribute : [aAttribute];
  this.mValues      = aValue.splice ? aValue : [aValue];
  this.mRemoveFlags = aRemoveFlag.splice ? aRemoveFlag : [aRemoveFlag];
  this.mUndoValues  = [];
} 
 
vxChangeAttributeTxn.prototype = {
  doTransaction: function ()
  {
    for (var i = 0; i < this.mAttributes.length; i++) {
      _ddf("this attribute", this.mAttributes[i]);
      this.mUndoValues[i] = this.mElement.getAttribute(this.mAttributes[i]);
      if (!this.mRemoveFlags[i])
        this.mElement.setAttribute(this.mAttributes[i], this.mValues[i]);
      else
        this.mElement.removeAttribute(this.mAttributes[i]);
    }
  },

  undoTransaction: function ()
  {
    for (var i = 0; i < this.mAttributes.length; i++) {
      if (this.mUndoValues[i]) 
        this.mElement.setAttribute(this.mAttributes[i], this.mUndoValues[i]);
      else
        this.mElement.removeAttribute(this.mAttributes[i]);
    }
  },
  
  redoTransaction: function ()
  {
    for (var i = 0; i < this.mAttributes.length; i++) {
      if (!this.mRemoveFlags[i])
        this.mElement.setAttribute(this.mAttributes[i], this.mValues[i]);
      else
        this.mElement.removeAttribute(this.mAttributes[i]);
    }
  },
  
  // XXX TODO: update to support multiple attribute syntax
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

