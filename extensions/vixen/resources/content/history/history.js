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
 
var vxHistory = 
{
  mDocumentID: null, 
  mBundle: null,
  
  startup: function ()
  {
    // add a focus observer so that we can update our content depending on which
    // document is at front
    var rootShell = vxUtils.getRootShell();
    if (rootShell && "observerService" in rootShell)
      rootShell.observerService.addObserver(vfdFocusObserver, "vfd-focus", false);
   
    this.mBundle = document.getElementById("historyBundle");
  }
};

var vfdFocusObserver = {
  Observe: function (aSubject, aTopic, aData) 
  {
    // only update if we need to switch datasources. 
    _ddf(aData.mDocumentID, vxHistory.mDocumentID);
    if (aData.mDocumentID === vxHistory.mDocumentID) {
      _dd("Not re-rooting for the same ID");
      return false;
    }
    vxHistory.mDocumentID = aData.mDocumentID;
  
    var historyTree = document.getElementById("historyTree");
    if (historyTree) {
      var datasources = historyTree.database.GetDataSources();
      while(datasources.hasMoreElements()) {
        var currDS = datasources.getNext();
        currDS = currDS.QueryInterface(Components.interfaces.nsIRDFDataSource);
        historyTree.database.RemoveDataSource(currDS);
      }
      var datasource = aSubject.QueryInterface(Components.interfaces.nsIRDFDataSource);
      if (datasource) {
        historyTree.database.AddDataSource(datasource);
        historyTree.builder.rebuild();
        
        if (aData.vfdDocumentWindowNode) {
          var documentTitle = aData.vfdDocumentWindowNode.getAttribute("title");
          var titleString = vxHistory.mBundle.getFormattedString("historyWindowTitle",
                                                                 [ documentTitle ]);
          window.title = titleString;
        }
        
        /*
        // XXX - inspect the datasource
        var RDFService = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService();
        RDFService = RDFService.QueryInterface(Components.interfaces.nsIRDFService);
        gVxTransactionHead = RDFService.GetResource(kTxnHeadURI);
        gVxTransactionList = RDFService.GetResource(kVixenRDF + "transaction-list");
        var seq = datasource.GetTarget(gVxTransactionHead, gVxTransactionList, true);
        seq = seq.QueryInterface(Components.interfaces.nsIRDFResource);

        gVxTxnDescription = RDFService.GetResource(kVixenRDF + "description");
        var container = Components.classes["@mozilla.org/rdf/container;1"].getService();
        container = container.QueryInterface(Components.interfaces.nsIRDFContainer);
        container.Init(datasource, seq);
        var elts = container.GetElements();
        
        while (elts.hasMoreElements()) {
          var currElt = elts.getNext();
          currElt = currElt.QueryInterface(Components.interfaces.nsIRDFResource);
          var commandString = datasource.GetTarget(currElt, gVxTxnDescription, true);
          commandString = commandString.QueryInterface(Components.interfaces.nsIRDFLiteral);
          _ddf("commandString", commandString.Value);
        }
        */
      }
    }
    return true;
  }
};

function requestUndo ()
{
  _dd("requestUndo");
  var rootShell = vxUtils.getRootShell();
  if (rootShell) rootShell.undo();
}

function requestRedo ()
{
  _dd("requestRedo");
  var rootShell = vxUtils.getRootShell();
  if (rootShell) rootShell.redo();
}

var historyDNDObserver = 
{
  onDragStart: function ()
  {
    var tree = document.getElementById("historyTree");
    if (!tree.selectedItems.length) return null;
    
    var commandString = "";
    for (var i = 0; i < tree.selectedItems.length; ++i) {
      var currItem = tree.selectedItems[i];
      if (currItem)
        commandString += currItem.getAttribute("command");
    }      
    
    _ddf("command-string dragged", commandString);

    var flavourList = { };
    flavourList["text/x-moz-vixen-command"] = { width: 2, data: commandString };
    return flavourList;
  }
};  

