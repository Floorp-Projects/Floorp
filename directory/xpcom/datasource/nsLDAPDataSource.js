/* 
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
 * The Original Code is the mozilla.org LDAP RDF datasource.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): Dan Mosedale <dmose@mozilla.org>
 *		   Brendan Eich <brendan@mozilla.org>
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

const DEBUG = true;

// core RDF schema
//
const RDF_NAMESPACE_URI = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
const NC_NAMESPACE_URI = "http://home.netscape.com/NC-rdf#";

// RDF-specific success nsresults 
//
const NS_ERROR_RDF_BASE = 0x804f0000; // XXX is this right?
const NS_RDF_CURSOR_EMPTY = NS_ERROR_RDF_BASE + 1;
const NS_RDF_NO_VALUE = NS_ERROR_RDF_BASE + 2;
const NS_RDF_ASSERTION_ACCEPTED = Components.results.NS_OK;
const NS_RDF_ASSERTION_REJECTED = NS_ERROR_RDF_BASE + 3;

// some stuff from ldap.h
//
const LDAP_RES_BIND = 0x61;

// ArrayEnumerator courtesy of Brendan Eich <brendan@mozilla.org>
//
function ArrayEnumerator(array) {
    this.array = array;
    this.index = 0;
}
ArrayEnumerator.prototype = {
    hasMoreElements: function() { return this.index < this.array.length; },
    getNext: function () { return (this.index < this.array.length) ?
			           this.array[this.index++] : null; }
}

// the datasource object itself
//
const NS_LDAPDATASOURCE_CONTRACTID = 
    '@mozilla.org/rdf/datasource;1?name=ldap';
const NS_LDAPDATASOURCE_CID =
    Components.ID('{8da18684-6486-4a7e-b261-35331f3e7163}');

function nsLDAPDataSource() {}

nsLDAPDataSource.prototype = {

    // from nsIRDFRemoteDataSource.  right now this is just a dump()
    // to see if it ever even gets used.
    //
    get loaded() { dump("getter for loaded called\n"); 
		   return false; },

    mObserverList: new Array(),	// who is watching us; XXX ok to use new Array?

    // RDF property constants.  
    //
    // XXXdmose - can these can actually be statics (ie JS class
    // properties rather than JS instance properties)?  or would that
    // make it hard for the datasource and/or component to be GCed?
    //
    kRDF_instanceOf: {},
    kNC_child: {},

    // since we implement multiple interfaces, we have to provide QI
    //
    QueryInterface: function(iid) {

      if (!iid.equals(Components.interfaces.nsISupports) &&
	  !iid.equals(Components.interfaces.nsIRDFDataSource) &&
	  !iid.equals(Components.interfaces.nsIRDFRemoteDataSource) &&
	  !iid.equals(Components.interfaces.nsILDAPMessageListener))
	  throw Components.results.NS_ERROR_NO_INTERFACE;

      return this;
    },

    /**
     * for nsIRDFRemoteDataSource
     */
    Init: function(aServer, aPort, aBindname)
    {

	// XXX - if this a "single-connection" datasource; we should figure
	// that out here by noticing that there is a ; after "rdf:ldap", and
	// that after that there is a cookie which happens to be an LDAP URL
	// designating the connection.  for now, we just assume that this will
	// be a "universal" datasource.

	// get the RDF service
	//
	var rdfSvc = Components.
	    classes["@mozilla.org/rdf/rdf-service;1"].
	    getService(Components.interfaces.nsIRDFService);

	// get some RDF Resources that we'll need
	//
	this.kRDF_instanceOf = rdfSvc.GetResource( RDF_NAMESPACE_URI +
						   "instanceOf");
	this.kNC_child = rdfSvc.GetResource( NC_NAMESPACE_URI + "child");
        this.kNC_Folder = rdfSvc.GetResource( NC_NAMESPACE_URI + "Folder");

	return;
    },

    /**
     * Add an observer to this data source. If the datasource
     * supports observers, the datasource source should hold a strong
     * reference to the observer. (from nsIRDFDataSource)
     *
     * void AddObserver(in nsIRDFObserver aObserver);
     */
    AddObserver: function(aObserver) {
	this.mObserverList.push(aObserver);
    },

    /**
     * Query whether an assertion exists in this graph. (from nsIRDFDataSource)
     *
     * boolean HasAssertion(in nsIRDFResource aSource,
     *	                    in nsIRDFResource aProperty,
     *                      in nsIRDFNode     aTarget,
     *                      in boolean        aTruthValue);
     */
    HasAssertion: function(aSource, aProperty, aTarget, aTruthValue) {

	// figure out what kind of nsIRDFNode aTarget is
	// XXXdmose: code incomplete
	//
        aTarget = aTarget.QueryInterface(Components.interfaces.nsIRDFResource);

	// spew debugging info
	//
	if (DEBUG) {
	    dump("HasAssertion() called with args: \n\t" + aSource.Value +
		 "\n\t" + aProperty.Value + "\n\t" + aTarget.Value + "\n\t" +
		 aTruthValue + "\n\n");
	}

	// the datasource doesn't currently use any sort of containers
	//
	if (aProperty.EqualsNode(this.kRDF_instanceOf)) {
	    return false;
	}
       
	// XXXdmose: real processing should happen here
	//
	var myMessage = aSource.GetDelegate("ldapmessage", Components.
					    interfaces.nsILDAPMessage);

	// if we haven't returned true yet, there's nothing here
        //
	return false;
    },

    /**
     * Find a child of that is related to the source by the given arc
     * arc and truth value
     *
     * @return NS_RDF_NO_VALUE if there is no target accessable from the
     * source via the specified property.
     *
     *    nsIRDFNode GetTarget(in nsIRDFResource aSource,
     *                         in nsIRDFResource aProperty,
     *                         in boolean aTruthValue);
     */
    GetTarget: function(aSource, aProperty, aTruthValue) 
    {
	if (DEBUG) {
	    dump("GetTarget() called with args: \n\t" + aSource.Value + 
		 "\n\t" + aProperty.Value + "\n\t" + aTruthValue + "\n\n");
	}

	Components.returnCode = NS_RDF_NO_VALUE;

	return null;
    },

    /**
     * Find all children of that are related to the source by the given arc
     * arc and truth value.
     *
     * @return NS_OK unless a catastrophic error occurs. If the
     * method returns NS_OK, you may assume that nsISimpleEnumerator points
     * to a valid (but possibly empty) cursor.
     *
     * nsISimpleEnumerator GetTargets(in nsIRDFResource aSource,
     *                                in nsIRDFResource aProperty,
     *                                in boolean aTruthValue);
     */

     GetTargets: function(aSource, aProperty, aTruthValue) {

	 function generateGetTargetsBoundCallback() {

	     function getTargetsBoundCallback() {}

	     getTargetsBoundCallback.prototype.onLDAPMessage = 

	         function(aMessage, aRetVal) {

		     if (DEBUG) {
			 dump("boundCallback() called with scope: \n\t" +
			      aSource.Value + "\n\t" + aProperty.Value + 
			      "\n\t" + aTruthValue + "\n");
			 dump("\taRetVal = " + aRetVal + "\n\n");
		     }

		     // XXX how do we deal with this in release builds?
		     // XXX deal with already bound case
		     //
		     if (aRetVal != LDAP_RES_BIND) {
			 dump("bind failed\n");
		     }

		     // kick off a search
		     //
		     var searchOp = Components.classes[
			 "mozilla.network.ldapoperation"].
		             createInstance(Components.interfaces.
			         nsILDAPOperation);
		     // XXX err handling
		     searchOp.init(connection, 
				   generateGetTargetsSearchCallback());
		     // XXX err handling (also for url. accessors)
		     // XXX constipate this
		     // XXX real timeout
		     searchOp.searchExt(url.dn, url.scope, url.filter, 0, -1);
		 }

	     return new getTargetsBoundCallback();
	 }

	 function generateGetTargetsSearchCallback() {

	     function getTargetsSearchCallback() {}

	     getTargetsSearchCallback.prototype.onLDAPMessage = 

	         function(aMessage, aRetVal ) {
		     dump("getTargetsSearchCallback() called with aRetVal=" +
			  aRetVal + "\n\n");
		 }
	     return new getTargetsSearchCallback();
	 }

	 if (DEBUG) {
	     dump("GetTargets() called with args: \n\t" + aSource.Value + 
		  "\n\t" + aProperty.Value + "\n\t" + aTruthValue + "\n\n");
	 }

	 var url = Components.classes["mozilla.network.ldapurl"]
	                        .getService(Components.interfaces.nsILDAPURL);
	 url.spec = aSource.Value;
	 
	 // get a connection object
	 //
	 var connection = Components.classes
	                   ["mozilla.network.ldapconnection"].createInstance(
			   Components.interfaces.nsILDAPConnection);
	 connection.init(url.host, url.port, null);

	 // get and initialize an operation object
	 //
	 var operation = Components.classes["mozilla.network.ldapoperation"].
	                     createInstance(Components.interfaces.
					    nsILDAPOperation);
	 operation.init(connection, generateGetTargetsBoundCallback());

	 // bind to the server.  we'll get a callback when this finishes.
	 // XXX handle a password
	 //
	 operation.simpleBind(null);

	 return new ArrayEnumerator(new Array());
     },

    /**
     * Get a cursor to iterate over all the arcs that originate in
     * a resource.
     *
     * @return NS_OK unless a catastrophic error occurs. If the method
     * returns NS_OK, you may assume that labels points to a valid (but
     * possible empty) nsISimpleEnumerator object.
     *
     * nsISimpleEnumerator ArcLabelsOut(in nsIRDFResource aSource);
     */
    ArcLabelsOut: function(aSource) 
    {
	if (DEBUG) {
	    dump("ArcLabelsOut() called with args: \n\t" + aSource.Value + 
		 "\n\n");
	}

	return new ArrayEnumerator(new Array());
    }
}

// the nsILDAPMessage associated with a given resource
//
const NS_LDAPMESSAGERDFDELEGATEFACTORY_CONTRACTID = 
    '@mozilla.org/rdf/delegate-factory/message.ldap;1';
const NS_LDAPMESSAGERDFDELEGATEFACTORY_CID = 
    Components.ID('{4b6fb566-1dd2-11b2-a1a9-889a3f852b0b`}');

function nsLDAPMessageRDFDelegateFactory() {}

nsLDAPMessageRDFDelegateFactory.prototype = 
{

    mConnection: {},		// connection to the LDAP server
    mOperation: {}, 	 	// current LDAP operation

    // from nsIRDFDelegateFactory:
    //
    // Create a delegate for the specified RDF resource.
    //
    // The created delegate should forward AddRef() and Release()
    // calls to the aOuter object.
    //
    // void CreateDelegate(in nsIRDFResource aOuter,
    //                     in string aKey,
    //                     in nsIIDRef aIID,
    //                     [retval, iid_is(aIID)] out nsQIResult aResult);
    CreateDelegate: function (aOuter, aKey, aIID) {

	

    }

}

// the nsILDAPURL associated with a given resource
//
const NS_LDAPURLRDFDELEGATEFACTORY_CONTRACTID = '@mozilla.org/rdf/delegate-factory/url.ldap;1';
const NS_LDAPURLRDFDELEGATEFACTORY_CID = 
    Components.ID('b6048700-1dd1-11b2-ae88-a5e18bb1f25e');

// the nsILDAPConnection associated with a given resource
//
const NS_LDAPCONNECTIONRDFDELEGATEFACTORY_CONTRACTID = 
    '@mozilla.org/rdf/delegate-factory/connection.ldap;1';
const NS_LDAPCONNECTIONRDFDELEGATEFACTORY_CID = 
    Components.ID('57075fc6-1dd2-11b2-9df5-dbb9111d1b38');

// nsLDAPDataSource Module (for XPCOM registration)
// 
var nsLDAPDataSourceModule = {

    registerSelf: function (compMgr, fileSpec, location, type) {

        compMgr.registerComponentWithType(NS_LDAPDATASOURCE_CID, 
					  'LDAP RDF DataSource', 
					  NS_LDAPDATASOURCE_CONTRACTID, 
					  fileSpec, location, true, true, 
					  type);

        compMgr.registerComponentWithType(
	    NS_LDAPMESSAGERDFDELEGATEFACTORY_CID, 
	    'LDAP Message RDF Delegate', 
	    NS_LDAPMESSAGERDFDELEGATEFACTORY_CONTRACTID, 
	    fileSpec, location, true, true, 
	    type);

        compMgr.registerComponentWithType(NS_LDAPURLRDFDELEGATEFACTORY_CID, 
					  'LDAP URL RDF Delegate', 
					  NS_LDAPURLRDFDELEGATEFACTORY_CONTRACTID, 
					  fileSpec, location, true, true, 
					  type);

        compMgr.registerComponentWithType(
	    NS_LDAPCONNECTIONRDFDELEGATEFACTORY_CID, 
	    'LDAP Connection RDF Delegate', 
	    NS_LDAPCONNECTIONRDFDELEGATEFACTORY_CONTRACTID, 
	    fileSpec, location, true, true, 
	    type);

    },

    getClassObject: function(compMgr, cid, iid) {
        if (!iid.equals(Components.interfaces.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        if (cid.equals(NS_LDAPDATASOURCE_CID))
	    return this.nsLDAPDataSourceFactory;	
	else if (cid.equals(NS_LDAPMESSAGERDFDELEGATEFACTORY_CID))
            return this.nsLDAPMessageRDFDelegateFactoryFactory;
	else if (cid.equals(NS_LDAPURLRDFDELEGATEFACTORY_CID))
            return this.nsLDAPURLRDFDelegateFactoryFactory;
	else if (cid.equals(NS_LDAPCONNECTIONRDFDELEGATEFACTORY_CID))
            return this.nsLDAPConnectionRDFDelegateFactoryFactory;

	throw Components.results.NS_ERROR_NO_INTERFACE;
    },

    nsLDAPDataSourceFactory: {
      createInstance: function(outer, iid) {
	  if (outer != null)
	      throw Components.results.NS_ERROR_NO_AGGREGATION;
	  
	  return (new nsLDAPDataSource()).QueryInterface(iid);
      }
    },

    nsLDAPMessageRDFDelegateFactoryFactory: {
	createInstance: function(outer, iid) {
	  if (outer != null)
	      throw Components.results.NS_ERROR_NO_AGGREGATION;
	  
	  return (new nsLDAPMessageRDFDelegateFactory()).QueryInterface(iid);
      }
    },

    nsLDAPURLRDFDelegateFactoryFactory: {
	createInstance: function(outer, iid) {
	  if (outer != null)
	      throw Components.results.NS_ERROR_NO_AGGREGATION;
	  
	  return (new nsLDAPURLRDFDelegateFactory()).QueryInterface(iid);
      }
    },

    nsLDAPConnectionRDFDelegateFactoryFactory: {
	createInstance: function(outer, iid) {
	  if (outer != null)
	      throw Components.results.NS_ERROR_NO_AGGREGATION;
	  
	  return 
	      (new nsLDAPConnectionRDFDelegateFactory()).QueryInterface(iid);
      }
    },

    // because of the way JS components work (the JS garbage-collector
    // keeps track of all the memory refs and won't unload until appropriate)
    // this ends up being a dummy function; it can always return true.
    //
    canUnload: function(compMgr) { return true; }
};

function NSGetModule(compMgr, fileSpec) { return nsLDAPDataSourceModule; }
