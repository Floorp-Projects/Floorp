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
  startup: function ()
  {
    var rootWindow = vxUtils.getWindow("vixen:main");
    if (rootWindow) {
      _dd("adding observer");
      rootWindow.vxShell.observerService.AddObserver(vfdFocusObserver, "vfd-focus");
    }
  }
};

var vfdFocusObserver = {
  Observe: function (aSubject, aTopic, aData) 
  {
    _dd("do Observe");
    var historyTree = document.getElementById("historyTree");
    if (historyTree) {
      var datasources = historyTree.database.GetDataSources();
      while(datasources.hasMoreElements()) {
        var currDS = datasources.getNext();
        historyTree.database.RemoveDataSource(currDS);
      }
      var datasource = aSubject.QueryInterface(Components.interfaces.nsIRDFDataSource);
      if (datasource) {
        _dd("rebuilding history tree");
        _ddf("datasource", datasource);
        
        historyTree.database.AddDataSource(datasource);
        
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
        
      }
    }
  }
};


