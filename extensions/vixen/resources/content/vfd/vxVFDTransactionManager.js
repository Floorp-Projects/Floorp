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
  const kTxMgrContractID = "@mozilla.org/transaction/manager;1";
  const kTxMgrIID = "nsITransactionManager";
  this.mTxMgr = nsJSComponentManager.getService(kTxMgrContractID, kTxMgrIID);
}

vxVFDTransactionManager.prototype = 
{
  mTxMgr:   null,
  mDoSeq:   null,
  mUndoSeq: null,
  mTxnDS:   null,

  doTransaction: function (aTransaction) 
  {
    // XXX Until the txmgr is fully scriptable, we need to perform the
    //     transaction ourself 
    aTransaction.doTransaction();
    
    if (!this.mDoSeq) {
      // If a Transaction Seq does not exist, create one.
      this.makeSeq(vxVFDDoTransactionSeq);
    }
    _dd("we've successfully made a sequence\n");
    
    // add the transaction to the stack
    this.mDoSeq.AppendElement(aTransaction);
    
    if (!this.mTxMgr) {
      // If a Transaction Manager does not exist for this VFD, 
      // create one.
      const kTxMgrCONTRACTID = "@mozilla.org/transaction/manager;1";
      const kTxMgrIID = "nsITransactionManager";
      // XXX comment this out until this works
      // this.mTxMgr = nsJSComponentManager.getService(kTxMgrCONTRACTID, kTxMgrIID);
    }
    
    // do the transaction
    // XXX comment this out until this works
    // this.mTxMgr.doTransaction(aTransaction);
  },
  
  undoTransaction: function ()
  {  
    // XXX Until the txmgr is fully scriptable, we need to undo the
    //     transaction ourself 
    aTransaction.undoTransaction();
    
    if (!this.mUndoSeq) {
      // If a  Transaction Undo Seq does not exist, create one
      this.makeSeq(vxVFDUndoTransactionSeq);
    }
    
    // remove the transaction at the top of the do seq
    var lastIndex = this.mDoSeq.GetCount() - 1;
    var txn = this.mDoSeq.RemoveElementAt(lastIndex, true);
    
    // and add it to the undo seq
    this.mUndoSeq.AppendElement(txn);
    
    // undo the transaction
    // XXX comment this out until this works
    // this.mTxMgr.undoTransaction();
  },
  
  redoTransaction: function (aTransaction)
  {
    // XXX Until the txmgr is fully scriptable, we need to redo the
    //     transaction ourself 
    aTransaction.redoTransaction();
    
    // remove the transaction at the top of the undo seq
    var lastIndex = this.mUndoSeq.GetCount() - 1;
    var txn = this.mDoSeq.RemoveElementAt(lastIndex, true);
    
    // and add it to the do seq
    this.mDoSeq.AppendElement(txn);
    
    // redo the transaction
    // XXX comment this out until this works
    // this.mTxMgr.redoTransaction();
  },
  
  makeSeq: function (aResource)
  {
    const kContainerUtilsCID = "{d4214e92-fb94-11d2-bdd8-00104bde6048}";
    const kContainerUtilsIID = "nsIRDFContainerUtils";
    var utils = nsJSComponentManager.getServiceByID(kContainerUtilsCID, kContainerUtilsIID);
    this.mDoSeq = utils.MakeSeq(vxVFDTransactionDS, aResource);
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
    _ddf("vxtxds::hasassertion", aValue);
    var res = aSource.Value;
    if (!res) throw Components.results.NS_ERROR_FAILURE; 
    var prop = aProperty.Value;
    if (!prop) throw Components.results.NS_ERROR_FAILURE;
    _ddf("so, the values are", res + ", " + prop);
    
    if (this.mResources[res] && 
        this.mResources[res][prop] &&
        this.mResources[res][prop].EqualsNode(aValue))
      return true;
    return false;
  },
  
  Assert: function (aSource, aProperty, aValue, aTruthVal)
  {
    _dd("vxtxds::assert");
    var res = aSource.Value;
    if (!res) throw Components.results.NS_ERROR_FAILURE; 
    var prop = aProperty.Value;
    if (!prop) throw Components.results.NS_ERROR_FAILURE;
    
    this.mResources[res] = { };
    _ddf("going to assert into", res + ", " + prop);
    this.mResources[res][prop] = aValue;
    _ddf("value is now", this.mResources[res][prop]);
  },
  
  Unassert: function (aSource, aProperty, aValue, aTruthVal)
  {
    _dd("vxtxds::unassert");
    var res = aSource.Value;
    if (!res) throw Components.results.NS_ERROR_FAILURE;
    var prop = aSource.Value;
    if (!prop) throw Components.results.NS_ERROR_FAILURE;
    if (!aValue) throw Components.results.NS_ERROR_FAILURE;
    
    if (!this.mResources[res][prop])
      throw Components.results.NS_ERROR_FAILURE;
    
    this.mResources[res][prop] = undefined;
  },
  
  GetTarget: function (aSource, aProperty, aTruthValue)
  {
    _dd("vxtxds::gettarget");
    var res = aSource.Value;
    if (!res) throw Components.results.NS_ERROR_FAILURE;
    var prop = aProperty.Value;
    if (!prop) throw Components.results.NS_ERROR_FAILURE;

    if (this.mResources[res] != undefined && 
        this.mResources[res][prop] != undefined) {
      var thang = this.mResources[res][prop].QueryInterface(Components.interfaces.nsIRDFLiteral);
      _ddf("prop", thang.Value);
      return this.mResources[res][prop];
    }
    throw Components.results.NS_ERROR_FAILURE;
    return null;
  }
};

/** 
 * Implements nsIRDFResource
 */
var vxVFDDoTransactionSeq = 
{
  Value: "vxVFDDoTransactionSeq"
};

var vxVFDUndoTransactionSeq = 
{
  Value: "vxVFDUndoTransactionSeq"
};


