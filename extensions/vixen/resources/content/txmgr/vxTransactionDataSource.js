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

// Global Resource URIs
const kTxnHeadURI = "urn:mozilla:vixen:transactions";
const kTxnURI     = "urn:mozilla:vixen:transaction:"; 
const kTxnRootURI = kTxnURI + "root";
const kVixenRDF = "http://www.mozilla.org/projects/vixen/rdf#";

// Global Resources
var gVxTransaction        = null;   // meta resource for transaction information
var gVxTransactionList    = null;   // arc to
var gVxTransactionRoot    = null;   // transaction list

var gVxTxnCommandString   = null;   // command string for a transaction
var gVxTxnDescription     = null;   // pretty description of a transaction
var gVxTxnState           = null;   // values = "undo"/"redo"
var gVxTxnStackIndex      = null;   // specifies the stack index (current top transaction)
var gVxTxnPosition        = null;  // specifies the position of the txn (top/bottom of stack, etc);

function vxTransactionDataSource()
{
  // Obtain the RDF Service
  const kRDFServiceCONTRACTID = "@mozilla.org/rdf/rdf-service;1";
  this.mRDFS = Components.classes[kRDFServiceCONTRACTID].getService();
  if (!this.mRDFS) throw Components.results.NS_ERROR_FAILURE;
  this.mRDFS = this.mRDFS.QueryInterface(Components.interfaces.nsIRDFService);
  if (!this.mRDFS) throw Components.results.NS_ERROR_FAILURE;

  // Create our internal in-memory datasource
  const kRDFIMDS = "@mozilla.org/rdf/datasource;1?name=in-memory-datasource";
  this.mDataSource = Components.classes[kRDFIMDS].createInstance();
  if (!this.mDataSource) throw Components.results.NS_ERROR_FAILURE;
  this.mDataSource = this.mDataSource.QueryInterface(Components.interfaces.nsIRDFDataSource);
  
  // Initialize the global resources
  gVxTransaction      = this.mRDFS.GetResource(kTxnHeadURI);
  gVxTransactionList  = this.mRDFS.GetResource(kVixenRDF + "transaction-list");
  gVxTransactionRoot  = this.mRDFS.GetResource(kTxnRootURI); 

  // Assert the datasource structure into the graph
  this.mDataSource.Assert(gVxTransaction, 
                          gVxTransactionList,
                          gVxTransactionRoot, true);

  // initialize other resources
  gVxTxnCommandString = this.mRDFS.GetResource(kVixenRDF + "command-string");
  gVxTxnDescription   = this.mRDFS.GetResource(kVixenRDF + "description");                            
  gVxTxnState         = this.mRDFS.GetResource(kVixenRDF + "state");
  gVxTxnStackIndex    = this.mRDFS.GetResource(kVixenRDF + "stack-index");
  gVxTxnPosition      = this.mRDFS.GetResource(kVixenRDF + "position");
}

vxTransactionDataSource.prototype = 
{
  HasAssertion: function (aSource, aProperty, aValue, aTruthValue)
  {
    return this.mDataSource.HasAssertion(aSource, aProperty, aValue, aTruthValue);
  },
  
  Assert: function (aSource, aProperty, aValue, aTruthValue)
  {
    this.mDataSource.Assert(aSource, aProperty, aValue, aTruthValue);
  },
  
  Unassert: function (aSource, aProperty, aValue, aTruthValue)
  {
    this.mDataSource.Unassert(aSource, aProperty, aValue, aTruthValue);
  },

  Change: function (aSource, aProperty, aOldVal, aNewVal) 
  {
    this.mDataSource.Change(aSource, aProperty, aOldVal, aNewVal);
  },
  
  GetTarget: function (aSource, aProperty, aTruthValue)
  {
    return this.mDataSource.GetTarget(aSource, aProperty, aTruthValue);
  },
  
  GetTargets: function (aSource, aProperty, aTruthValue)
  {
    return this.mDataSource.GetTargets(aSource, aProperty, aTruthValue);
  },
  
  GetSource: function (aProperty, aTarget, aTruthValue)
  {
    return this.mDataSource.GetSource(aProperty, aTarget, aTruthValue);
  },
  
  GetSources: function (aProperty, aTarget, aTruthValue)
  {
    return this.mDataSource.GetSources(aProperty, aTarget, aTruthValue);
  },
  
  ArcLabelsIn: function (aNode)
  {
    return this.mDataSource.ArcLabelsIn(aNode);
  },
  
  ArcLabelsOut: function (aSource) 
  {
    return this.mDataSource.ArcLabelsOut(aSource);
  },
  
  AddObserver: function (aObserver) 
  {
    this.mDataSource.AddObserver(aObserver);
  },
  
  RemoveObserver: function (aObserver)
  {
    this.mDataSource.RemoveObserver(aObserver);
  }
};

