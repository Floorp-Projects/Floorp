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
 * vxAggregateTxn - Transaction that allows individual transactions to be
 *                  coalesced and done/undone in one operation
 * 
 * e.g., create a new element in the VFD document using the palette that
 *       has several attributes set on it.
 */

function vxAggregateTxn (aTransactionList)
{
  this.mTransactionList = aTransactionList;
} 
 
vxAggregateTxn.prototype = {
  doTransaction: function ()
  {
    _dd("vxAggregateTxn::doTransaction");
    for (var i = 0; i < this.mTransactionList.length; i++)
      this.mTransactionList[i].doTransaction();
  },

  undoTransaction: function ()
  {
    _dd("vxAggregateTxn::undoTransaction");
    for (var i = 0; i < this.mTransactionList.length; i++) 
      this.mTransactionList[i].undoTransaction();
  },
  
  redoTransaction: function ()
  {
    _dd("vxAggregateTxn::redoTransaction");
    for (var i = 0; i < this.mTransactionList.length; i++)
      this.mTransactionList[i].redoTransaction();
  },

  get commandString()
  {
    var commandString = "aggregate-txn";
    // XXX TODO: elaborate
    return commandString;    
  }
};

/** 
 * txnStack["create-element"]("button", someBox, 3);
 **/
 
 
 
 