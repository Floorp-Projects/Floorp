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
      this.mTxnSeq = this.makeSeq(vxVFDTransactionSeq);
    }

    if (this.mTxnStack.index < this.mTxnStack.mStack.length - 1) {
      // clear the stack from the index upwards
      this.mTxnStack.flush(this.mTxnStack.index);
      
      // likewise with the RDF Seq.
      var seqCount = this.mTxnSeq.GetCount();
      for (i = this.mTxnStack.index; i < seqCount; i++)
        this.mTxnSeq.RemoveElementAt(i);
    }
    
    // append the transaction to our list
    this.mTxnStack.push(aTransaction);

    // add the transaction to the stack
    this.mTxnSeq.AppendElement(new RDFLiteral(this.mTxnStack.index-1));
    
    for (i = 0; i < this.mTransactionListeners.length; i++) {
      interruptRequest = {};
      if ("didDo" in this.mTransactionListeners[i]) {
        _ddf("transaction listener " + i, this.mTransactionListeners[i].commandString);
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
    var txn = this.mTxnStack.mStack[this.mTxnStack.index-1];

    // undo the transaction
    if (txn) {
      _ddf("going to undo the txn at", this.mTxnStack.index-1);
      txn.undoTransaction();
      this.mTxnStack.index--;
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
    var txn = this.mTxnStack.mStack[this.mTxnStack.index];

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
    return utils.MakeSeq(vxVFDTransactionDS, aResource);
  },

  /**
   * add and remove nsITransactionListeners
   */
  addListener: function (aTransactionListener)
  {
    if ("splice" in aTransactionListener) {
      for (var i = 0; i < aTransactionListener.length; i++)
        this.mTransactionListeners.push(aTransactionListener[i]);
    }
    else
      this.mTransactionListeners.join(aTransactionListener);
    _ddf("transaction listeners :: ", this.mTransactionListeners.length);
  },
  
  removeListener: function (aTransactionListeners)
  {
    var listeners = [].join(aTransactionListeners)
    for (var i = 0; i < this.mTransactionListeners.length; i++) {
      for (var k = 0; k < listeners.length; k++) {
        if (this.mTransactionListeners[i] == listeners[k]) {
          this.mTransactionListeners.splice(i, 1);
          break;
        }
      }
    }
  }
};

/** 
 * Implements nsIRDFDataSource
 */
var vxVFDTransactionDS = 
{
  mResources: { },
 
  HasAssertion: function (aSource, aProperty, aValue, aTruthValue)
  {
    var res = aSource.Value;
    if (!res) throw Components.results.NS_ERROR_FAILURE; 
    var prop = aProperty.Value;
    if (!prop) throw Components.results.NS_ERROR_FAILURE;
    
    if (this.mResources[res] && 
        this.mResources[res][prop] &&
        this.mResources[res][prop].EqualsNode(aValue))
      return true;
    return false;
  },
  
  Assert: function (aSource, aProperty, aValue, aTruthVal)
  {
    var res = aSource.Value;
    if (!res) throw Components.results.NS_ERROR_FAILURE; 
    var prop = aProperty.Value;
    if (!prop) throw Components.results.NS_ERROR_FAILURE;
    
    if (!this.mResources[res])
      this.mResources[res] = { };
    this.mResources[res][prop] = aValue;
  },
  
  Unassert: function (aSource, aProperty, aValue, aTruthVal)
  {
    var res = aSource.Value;
    if (!res) throw Components.results.NS_ERROR_FAILURE;
    var prop = aProperty.Value;
    if (!prop) throw Components.results.NS_ERROR_FAILURE;
    if (!aValue) throw Components.results.NS_ERROR_FAILURE;

    if (!this.mResources[res][prop])
      throw Components.results.NS_ERROR_FAILURE;
    
    this.mResources[res][prop] = undefined;
  },
  
  GetTarget: function (aSource, aProperty, aTruthValue)
  {
    var res = aSource.Value;
    if (!res) throw Components.results.NS_ERROR_FAILURE;
    var prop = aProperty.Value;
    if (!prop) throw Components.results.NS_ERROR_FAILURE;

    if (this.mResources[res] != undefined && 
        this.mResources[res][prop] != undefined) {
      return this.mResources[res][prop].QueryInterface(Components.interfaces.nsIRDFNode);
    }
    throw Components.results.NS_ERROR_FAILURE;
    return null;
  },
  
  mObservers: [],
  AddObserver: function (aObserver) 
  {
    this.mObservers[this.mObservers.length] = aObserver;
  },
  
  RemoveObserver: function (aObserver)
  {
    for (var i = 0; i < this.mObservers.length; i++) {
      if (this.mObservers[i] == aObserver) {
        this.mObservers.splice(i, 1);
        break;
      }
    }
  }
};

/** 
 * Implements nsIRDFResource
 */
var vxVFDTransactionSeq = 
{
  Value: "vxVFDTransactionSeq"
};

function stripRDFPrefix (aString)
{
  var len = "http://www.w3.org/1999/02/22-rdf-syntax-ns#".length;
  return aString.substring(len, aString.length);
}

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
    _ddf("setting index to", aValue);
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

/** 
 * RDF Literal object
 */
function RDFLiteral (aValue)
{
  this.Value = aValue;
}

RDFLiteral.prototype = 
{
  EqualsNode: function (aRDFNode) 
  {
    try {
      var node = aRDFNode.QueryInterface(Components.interfaces.nsIRDFLiteral);
      if (node.Value == this.Value) 
        return true;
    }
    catch (e) {
    }
    return false;
  }
}

/*
function txnResource(aTransaction)
{
  this.Value = "";
  this.mTransaction = aTransaction;
}

txnResource.prototype = {  
  QueryInterface: function (aIID)
  {
    if (aIID == Components.interfaces.nsIRDFResource || 
        aIID == Components.interfaces.nsIRDFNode ||
        aIID == Components.interfaces.nsISupports)
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
    return null;
  }
};
*/

