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

var vxTransactionTable = { };
vxTransactionTable["create-element"]    = vxCreateElementTxnConstructor;
vxTransactionTable["change-attribute"]  = vxChangeAttributeTxnConstructor;
 
var vxTransactionDecoder = {
  decode: function (aTransactionString)
  {
    var txns = aTransactionString.split(";");
    for (var i = 0; i < txns.length; i++) {
      var currTxnName = this.getTransactionName (txns[i]);
      var fn = currTxnName == "aggregate-txn" ? this.decodeAggregate 
                                              : vxTransactionTable[currTxnName];
      fn (txns[i]);
    }
  },

/*      
aggregate-txn::{ mTransactions: [create-element::{
mLocalName: button};,change-attribute::{attributes: [value,id],values: [Button 2
,button_2],removeFlags: [false]};,] };aggregate-txn::{ mTransactions: [create-e
lement::{ mLocalName: button};,change-attribute::{attributes: [value,id],values:
 [Button 3,button_3],removeFlags: [false]};,] };
  },
 */
  
  decodeAggregate: function (aTransactionString)
  {
    var txnString = aTransactionString.substring(aTransactionString.indexOf("::"), 
                                                 aTransactionString.length);
    var txns = txnString.split("//");
    for (var i = 0; i < txns.length; i++) {
      var currTxnName = this.getTransactionName (txns[i]);
      var fn = currTxnName == "aggregate-txn" ? this.decodeAggregate 
                                              : vxTransactionTable[currTxnName];
      fn (txns[i]);
    }
  },
  
  getTransactionName: function (aTransactionString)
  {
    return aTransactionString.substring(0, aTransactionString.indexOf("::"));
  }
 
 };
 
 