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

  doTransaction: function (aTransaction) 
  {
    for (var i = 0; i < this.mTransactionListeners.length; i++) {
      var interruptRequest = {};
      if ("willDo" in this.mTransactionListeners[i]) {
        this.mTransactionListeners[i].willDo(this, aTransaction, interruptRequest);
        // do something with irq here once we figure out what it does.
      }
    }

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
      for (i = seqCount-1; i >= this.mTxnStack.index; i--)
        this.mTxnSeq.RemoveElementAt(i, true);
    }
    
    // append the transaction to our list
    this.mTxnStack.push(aTransaction);

    var resource = this.mDataSource.mRDFS.GetResource(kTxnURI + (this.mTxnStack.index-1));
    // Set the Command string
    var commandLiteral = this.mDataSource.mRDFS.GetLiteral(aTransaction.commandString);
    this.mDataSource.Assert(resource, gVxTxnCommandString, commandLiteral, true);
    // Set the Description string
    var descriptionLiteral = this.mDataSource.mRDFS.GetLiteral(aTransaction.description);
    this.mDataSource.Assert(resource, gVxTxnDescription, descriptionLiteral, true);
    // Set the Transaction state to that of the top undo item (which has the bottom border)
    var lastundoStateLiteral = this.mDataSource.mRDFS.GetLiteral("last-undo");
    this.mDataSource.Assert(resource, gVxTxnState, lastundoStateLiteral, true);
    // Set this item as the top of the stack
    var topofStackLiteral = this.mDataSource.mRDFS.GetLiteral("top-of-stack");
    this.mDataSource.Assert(resource, gVxTxnPosition, topofStackLiteral, true);
    // Change the Transaction state of the previous item to be plain undo, not 'last-undo'
    var prevItem = this.mDataSource.mRDFS.GetResource(kTxnURI + (this.mTxnStack.index-2));
    var undoStateLiteral = this.mDataSource.mRDFS.GetLiteral("undo");
    this.mDataSource.Change(prevItem, gVxTxnState, lastundoStateLiteral, undoStateLiteral);
    // Change the Transaction position of the previous item so it's not top of stack
    this.mDataSource.Unassert(prevItem, gVxTxnPosition, topofStackLiteral, true);
    // Set the Transaction Stack Index
    var indexLiteral = this.mDataSource.mRDFS.GetLiteral(this.mTxnStack.index);
    this.mDataSource.Assert(resource, gVxTxnStackIndex, indexLiteral, true);
    
    // Append to the transaction sequence. 
    this.mTxnSeq.AppendElement(resource);
    
    for (i = 0; i < this.mTransactionListeners.length; i++) {
      interruptRequest = {};
      if ("didDo" in this.mTransactionListeners[i]) {
        this.mTransactionListeners[i].didDo(this, aTransaction, interruptRequest);
        // do something with irq here once we figure out what it does.
      }
    }
  },
  
  undoTransaction: function ()
  {  
    for (var i = 0; i < this.mTransactionListeners.length; i++) {
      var interruptRequest = {};
      if ("willUndo" in this.mTransactionListeners[i]) {
        this.mTransactionListeners[i].willUndo(this, aTransaction, interruptRequest);
        // do something with irq here once we figure out what it does.
      }
    }
    
    // retrieve the previous transaction
    var txn = null;
    if (this.mTxnStack.index-1 >= 0)
      txn = this.mTxnStack.mStack[this.mTxnStack.index-1];

    // undo the transaction
    if (txn) {
      txn.undoTransaction();
      
      var resource = this.mDataSource.mRDFS.GetResource(kTxnURI + (this.mTxnStack.index-1));
      // Set the Transaction state (undo/redo)
      var undoStateLiteral = this.mDataSource.mRDFS.GetLiteral("undo");
      var redoStateLiteral = this.mDataSource.mRDFS.GetLiteral("redo");
      this.mDataSource.Change(resource, gVxTxnState, undoStateLiteral, redoStateLiteral);
      // Set the Transaction Stack Index
      var indexLiteral = this.mDataSource.mRDFS.GetLiteral(this.mTxnStack.index--);
      this.mDataSource.Assert(resource, gVxTxnStackIndex, indexLiteral, true);
      // Set the state of the previous item to be last-undo. 
      var prevItem = this.mDataSource.mRDFS.GetResource(kTxnURI + (this.mTxnStack.index-1));
      var lastUndoItemLiteral = this.mDataSource.mRDFS.GetLiteral("last-undo");
      var oldStateLiteral = this.mDataSource.GetTarget(prevItem, gVxTxnState, true);
      this.mDataSource.Change(prevItem, gVxTxnState, oldStateLiteral, lastUndoItemLiteral);
    }

    for (i = 0; i < this.mTransactionListeners.length; i++) {
      interruptRequest = {};
      if ("didUndo" in this.mTransactionListeners[i]) {
        this.mTransactionListeners[i].didUndo(this, aTransaction, interruptRequest);
        // do something with irq here once we figure out what it does.
      }
    }
  },
  
  redoTransaction: function (aTransaction)
  {
    for (var i = 0; i < this.mTransactionListeners.length; i++) {
      var interruptRequest = {};
      if ("willRedo" in this.mTransactionListeners[i]) {
        this.mTransactionListeners[i].willRedo(this, aTransaction, interruptRequest);
        // do something with irq here once we figure out what it does.
      }
    }

    // retrieve the previous transaction
    var txn = null;
    if (this.mTxnStack.index < this.mTxnStack.mStack.length)
      txn = this.mTxnStack.mStack[this.mTxnStack.index];

    // redo the transaction
    if (txn) { 
      txn.redoTransaction();
      this.mTxnStack.index++;
    }

    for (i = 0; i < this.mTransactionListeners.length; i++) {
      interruptRequest = {};
      if ("didRedo" in this.mTransactionListeners[i]) {
        this.mTransactionListeners[i].didRedo(this, aTransaction, interruptRequest);
        // do something with irq here once we figure out what it does.
      }
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


