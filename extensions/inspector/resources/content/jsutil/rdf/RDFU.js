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
    this.mListener.onError(aErrorMsg);
  },
  
  onEndLoad: function(aSink) 
  { 
    var ds = XPCU.QI(aSink, "nsIRDFDataSource");
    this.mListener.onDataSourceReady(ds);
	}
  
};
