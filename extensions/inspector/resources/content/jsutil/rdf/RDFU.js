/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 */

/***************************************************************
* RDFU -----------------------------------------------
*  Convenience routines for common RDF commands.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
*   chrome://inspector/content/jsutil/xpcom/XPCU.js
****************************************************************/

//////////// global constants ////////////////////

try {
var gRDF = Components.classes['@mozilla.org/rdf/rdf-service;1'].getService();
gRDF = gRDF.QueryInterface(Components.interfaces.nsIRDFService);

var gRDFCU = Components.classes['@mozilla.org/rdf/container-utils;1'].getService();
gRDFCU = gRDFCU.QueryInterface(Components.interfaces.nsIRDFContainerUtils);
} catch (ex) { alert("RDFU: " + ex);  }
///////////////////////////////////////////////////

var RDFU = {
  
  getSeqElementAt: function(aSeq, aIndex)
  {
  	var ordinal = gRDFCU.IndexToOrdinalResource(aIndex+1);
    return aSeq.DataSource.GetTarget(aSeq.Resource, ordinal, true);
  },
  
  readAttribute: function(aDS, aRes, aName)
  {
  	var attr = aDS.GetTarget(aRes, gRDF.GetResource(aName), true);
    if (attr)
    	attr = XPCU.QI(attr, "nsIRDFLiteral");
  	return attr ? attr.Value : null;
  },
  
  
  writeAttribute: function(aDS, aRes, aName, aValue)
  {
  	var attr = aDS.GetTarget(aRes, gRDF.GetResource(aName), true);
    if (attr)
    	aDS.Change(aRes, gRDF.GetResource(aName), attr, gRDF.GetLiteral(aValue));
  },
  
  
  findSeq: function(aDS, aResName)
  {
  	try {
      var res = gRDF.GetResource(aResName);
      seq = this.makeSeq(aDS, res);
    } catch (ex) { 
      alert("Unable to find sequence:" + ex); 
    }
  
    return seq;
  },
  
  makeSeq: function(aDS, aRes)
  {
    var seq = XPCU.createInstance("@mozilla.org/rdf/container;1", "nsIRDFContainer");
  	seq.Init(aDS, aRes);
    return seq;
  },
  
  createSeq: function(aDS, aBaseRes, aArcRes)
  {
    var res = gRDF.GetAnonymousResource();
    aDS.Assert(aBaseRes, aArcRes, res, true);
    var seq = gRDFCU.MakeSeq(aDS, res); 
    return seq;
  },
  
  loadDataSource: function(aURL, aListener) 
  {
  	var ds = gRDF.GetDataSource(aURL);
    var rds = XPCU.QI(ds, "nsIRDFRemoteDataSource");
    
    var observer = new DSLoadObserver(aListener);
    
    if (rds.loaded) {
    	observer.onEndLoad(ds);
    } else {
      var sink = XPCU.QI(ds, "nsIRDFXMLSink");
      sink.addXMLSinkObserver(observer);
    }
  },
  
  saveDataSource: function(aDS)
  {
    var ds = XPCU.QI(aDS, "nsIRDFRemoteDataSource");
    ds.Flush();
  }
};

///////////

function DSLoadObserver(aListener) { this.mListener = aListener; }

DSLoadObserver.prototype = 
{
  onBeginLoad: function(aSink) { },
  onInterrupt: function(aSink) {},
  onResume: function(aSink) {},
  onError: function(aSink, aStatus, aErrorMsg) 
  { 
    this.mListener.onError(aStatus, aErrorMsg);
  },
  
  onEndLoad: function(aSink) 
  { 
    var ds = XPCU.QI(aSink, "nsIRDFDataSource");
    this.mListener.onDataSourceReady(ds);
	}
  
};
