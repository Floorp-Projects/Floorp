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
 
function vxVFDTransactionManager() 
{
  this.mTxnStack = new vxVFDTransactionStack();
  this.mDataSource = new vxTransactionDataSource();
 
  // a record of transaction execution listeners that have attached
  // themselves to us.
  this.mTransactionListeners = [];
}

vxVFDTransactionManager.prototype = 
{
  mTxMgr:     null,
  mTxnSeq:    null,
  mTxnDS:     null,
  mTxnStack:  null,

  mTransactionListeners: null,

  setProperty: function (aSource, aProperty, aValue)
  {
    var valueAsLiteral = this.mDataSource.mRDFS.GetLiteral(aValue);
    var currValue = this.mDataSource.GetTarget(aSource, aProperty, true);
    if (currValue)
      this.mDataSource.Change(aSource, aProperty, currValue, valueAsLiteral);
    else
      this.mDataSource.Assert(aSource, aProperty, valueAsLiteral, true);
  },
  
  unsetProperty: function (aSource, aProperty, aValue) 
  {
    var currValue = !aValue ? this.mDataSource.GetTarget(aSource, aProperty, true) 
                            : this.mDataSource.mRDFS.GetLiteral(aValue);
    this.mDataSource.Unassert(aSource, aProperty, currValue, true);
  },

  getResourceForIndex: function (aIndex)
  {
    return this.mDataSource.mRDFS.GetResource(kTxnURI + aIndex);
  },
  
  notifyListeners: function (aEvent, aTransaction, aIRQ) 
  {
    for (var i = 0; i < this.mTransactionListeners.length; i++) {
      if (aEvent in this.mTransactionListeners[i]) {
        this.mTransactionListeners[i][aEvent](this, aTransaction, aIRQ);
        // do something with irq here once we figure out what it does.
      }
    }
  },
  
  doTransaction: function (aTransaction) 
  {
    this.notifyListeners("willDo", aTransaction, { });
  
    // perform the transaction
    aTransaction.doTransaction();
    
    if (!this.mTxnSeq) {
      // If a Transaction Seq does not exist, create one.
      this.mTxnSeq = this.makeSeq(gVxTransactionRoot);
    }

    if (this.mTxnStack.index < this.mTxnStack.mStack.length - 1) {
      // clear the stack from the index upwards
      this.mTxnStack.flush(this.mTxnStack.index);
      
      // likewise with the RDF Seq.
      var seqCount = this.mTxnSeq.GetCount();
      for (var i = seqCount-1; i >= this.mTxnStack.index; i--)
        this.mTxnSeq.RemoveElementAt(i, true);
    }
    
    // append the transaction to our list
    this.mTxnStack.push(aTransaction);

    var currItem = this.getResourceForIndex(this.mTxnStack.index-1);
    // Set the Command string
    this.setProperty(currItem, gVxTxnCommandString, aTransaction.commandString);
    // Set the Description string
    this.setProperty(currItem, gVxTxnDescription, aTransaction.description);
    // Set the Transaction state to that of the top undo item (which has the bottom border)
    this.setProperty(currItem, gVxTxnState, "last-undo");
    // Set this item as the top of the stack
    this.setProperty(currItem, gVxTxnPosition, "top-of-stack");
    // Change the Transaction state of the previous item to be plain undo, not 'last-undo'
    var prevItem = this.getResourceForIndex(this.mTxnStack.index-2);
    this.setProperty(prevItem, gVxTxnState, "undo");
    // Change the Transaction position of the previous item so it's not top of stack
    this.unsetProperty(prevItem, gVxTxnPosition, "top-of-stack");
    
    // Append to the transaction sequence. 
    this.mTxnSeq.AppendElement(currItem);
    
    this.notifyListeners("didDo", aTransaction, { });
  },
  
  undoTransaction: function ()
  {  
    // retrieve the previous transaction
    var txn = null;
    if (this.mTxnStack.index-1 >= 0)
      txn = this.mTxnStack.mStack[this.mTxnStack.index-1];

    // undo the transaction
    if (txn) {
      this.notifyListeners("willUndo", txn, { });
      txn.undoTransaction();
      
      var resource = this.getResourceForIndex(--this.mTxnStack.index);
      // Set the Transaction state (undo/redo)
      this.setProperty(resource, gVxTxnState, "redo");
      // Set the state of the previous item to be last-undo. 
      var prevItem = this.getResourceForIndex(this.mTxnStack.index-1);
      this.setProperty(prevItem, gVxTxnState, "last-undo");

      this.notifyListeners("didUndo", txn, { });
    }
  },
  
  redoTransaction: function ()
  {

    // retrieve the previous transaction
    var txn = null;
    if (this.mTxnStack.index < this.mTxnStack.mStack.length)
      txn = this.mTxnStack.mStack[this.mTxnStack.index];

    // redo the transaction
    if (txn) { 
      this.notifyListeners("willRedo", txn, { });
      txn.redoTransaction();

      var resource = this.getResourceForIndex(this.mTxnStack.index-1);
      // Set the Transaction state (undo/redo)
      this.setProperty(resource, gVxTxnState, "undo");
      // Set the state of the previous item to be last-undo. 
      var prevItem = this.getResourceForIndex(this.mTxnStack.index);
      this.setProperty(prevItem, gVxTxnState, "last-undo");

      ++this.mTxnStack.index;

      this.notifyListeners("didRedo", txn, { });
    }
  },
  
  makeSeq: function (aResource)
  {
    const kContainerUtilsCID = "{d4214e92-fb94-11d2-bdd8-00104bde6048}";
    const kContainerUtilsIID = "nsIRDFContainerUtils";
    var utils = nsJSComponentManager.getServiceByID(kContainerUtilsCID, kContainerUtilsIID);
    return utils.MakeSeq(this.mDataSource, aResource);
  },

  /**
   * add and remove nsITransactionListeners
   */
  addListener: function (aTransactionListener)
  {
    this.mTransactionListeners = this.mTransactionListeners.concat(aTransactionListener);
  },
  
  removeListener: function (aTransactionListener)
  {
    var listeners = [].concat(aTransactionListener);
    for (var i = 0; i < this.mTransactionListeners.length; ++i) {
      for (var k = 0; k < listeners.length; ++k) {
        if (this.mTransactionListeners[i] == listeners[k]) {
          this.mTransactionListeners.splice(i, 1);
          break;
        }
      }
    }
  }
};

/** 
 * Transaction stack
 */
function vxVFDTransactionStack()
{
  this.mIndex = 0;

  this.mStack = [];
}

vxVFDTransactionStack.prototype = 
{
  get index ()
  {
    return this.mIndex;
  },
  
  set index (aValue)
  {
    this.mIndex = aValue;
    return this.mIndex;
  },
  
  push: function (aTransaction) 
  {
    this.mStack.push(aTransaction);
    this.index++;
  },
  
  pop: function ()
  {
    this.index--;
    return this.mStack.pop();
  },
  
  flush: function (aStart)
  {
    if (aStart === undefined) 
      aStart = 0;
      
    this.mStack.splice(aStart, this.mStack.length - aStart);
  }
}; 


