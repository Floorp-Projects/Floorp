/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is the mozilla.org LDAP RDF datasource.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): Dan Mosedale <dmose@mozilla.org>
 *                 Brendan Eich <brendan@mozilla.org>
 *                 Peter Van der Beken <peter.vanderbeken@pandora.be>
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
const LDAPATTR_NAMESPACE_URI = "http://www.mozilla.org/LDAPATTR-rdf#";

// RDF-specific success nsresults 
//
const NS_ERROR_RDF_BASE = 0x804f0000; // XXX is this right?
const NS_RDF_CURSOR_EMPTY = NS_ERROR_RDF_BASE + 1;
const NS_RDF_NO_VALUE = NS_ERROR_RDF_BASE + 2;
const NS_RDF_ASSERTION_ACCEPTED = Components.results.NS_OK;
const NS_RDF_ASSERTION_REJECTED = NS_ERROR_RDF_BASE + 3;

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

// nsISupportsArrayEnumerator
//
function nsISupportsArrayEnumerator(array) {
    this.array = array;
    this.index = 0;
}
nsISupportsArrayEnumerator.prototype = {
    hasMoreElements: function() { return this.index < this.array.Count(); },
    getNext: function () { return (this.index < this.array.Count()) ?
                       this.array.GetElementAt(this.index++) : null; }
}

/**
 * getProxyOnUIThread returns a proxy to aObject on the main (UI) thread.
 * We need this because the RDF service should only be used on the main
 * thread. Moreover, the RDF classes aren't (marked as?) thread-safe, so
 * any objects we create using the RDF service should only be used on the
 * main thread.
 */
function getProxyOnUIThread(aObject, aInterface) {
    var eventQSvc = Components.
            classes["@mozilla.org/event-queue-service;1"].
            getService(Components.interfaces.nsIEventQueueService);
    var uiQueue = eventQSvc.
            getSpecialEventQueue(Components.interfaces.
            nsIEventQueueService.UI_THREAD_EVENT_QUEUE);
    var proxyMgr = Components.
            classes["@mozilla.org/xpcomproxy;1"].
            getService(Components.interfaces.nsIProxyObjectManager);

    return proxyMgr.getProxyForObject(uiQueue, 
            aInterface, aObject, 5); 
    // 5 == PROXY_ALWAYS | PROXY_SYNC
}


// the datasource object itself
//
const NS_LDAPDATASOURCE_CONTRACTID = 
    '@mozilla.org/rdf/datasource;1?name=ldap';
const NS_LDAPDATASOURCE_CID =
    Components.ID('{8da18684-6486-4a7e-b261-35331f3e7163}');

function nsLDAPDataSource() {}

nsLDAPDataSource.prototype = {

    // who is watching us; XXX ok to use new Array?
    mObserverList: new Array(),

    // RDF property constants.  
    //
    // XXXdmose - can these can actually be statics (ie JS class
    // properties rather than JS instance properties)?  or would that
    // make it hard for the datasource and/or component to be GCed?
    //
    kRDF_instanceOf: {},
    kNC_child: {},
    mRdfSvc: {},

    // since we implement multiple interfaces, we have to provide QI
    //
    QueryInterface: function(iid) {
      if (!iid.equals(Components.interfaces.nsISupports) &&
          !iid.equals(Components.interfaces.nsIRDFDataSource) &&
          !iid.equals(Components.interfaces.nsIRDFRemoteDataSource) &&
          !iid.equals(Components.interfaces.nsIRDFObserver))
      throw Components.results.NS_ERROR_NO_INTERFACE;

      return this;
    },

    Init: function(aURI)
    {
        if (DEBUG) {
            dump("Init() called with args: \n\t" + aURI + "\n\n");
        }

        // XXX - if this a "single-connection" datasource 
        // (ie non-anonymous to a specific server); we should figure
        // that out here by noticing that there is a ; after "rdf:ldap", and
        // that after that there is a cookie which happens to be an LDAP URL
        // designating the connection.  for now, we just assume that this will
        // be a "universal" (anonymous to any & all servers) datasource.  
        //
        // XXX rayw changed the delimiter char; it's not a ; any more.  
        // I think it might be ? now.  He or waterson would know.  The code
        // that knows about this is somewhere in rdf, perhaps in the XUL
        // template builder.

        // get the RDF service
        //
        this.mRdfSvc = Components.
                classes["@mozilla.org/rdf/rdf-service;1"].
                getService(Components.interfaces.nsIRDFService);

        // get some RDF Resources that we'll need
        //
        this.kRDF_instanceOf = this.mRdfSvc.GetResource( RDF_NAMESPACE_URI +
                "instanceOf");
        this.kNC_child = this.mRdfSvc.GetResource( NC_NAMESPACE_URI + "child");
        this.kNC_Folder = this.mRdfSvc.GetResource( NC_NAMESPACE_URI + 
                                                    "Folder");

        return;
    },

    /**
     * nsIRDFDataSource
     */

    /** Find an RDF resource that points to a given node over the
     * specified arc & truth value
     *
     * @return NS_RDF_NO_VALUE if there is no source that leads
     * to the target with the specified property.
     *
     * nsIRDFResource GetSource (in nsIRDFResource aProperty,
     *                           in nsIRDFNode aTarget,
     *                           in boolean aTruthValue);
     */
    GetSource: function(aProperty, aTarget, aTruthValue) {
        if (DEBUG) {
            dump("GetSource() called with args: \n\t" + aProperty.Value + 
             "\n\t" + aTarget.Value + "\n\t" + aTruthValue + "\n\n");
        }

        Components.returnCode = NS_RDF_NO_VALUE;
        return null;
    },

    /**
     * Find all RDF resources that point to a given node over the
     * specified arc & truth value
     *
     * @return NS_OK unless a catastrophic error occurs. If the
     * method returns NS_OK, you may assume that nsISimpleEnumerator points
     * to a valid (but possibly empty) cursor.
     *
     * nsISimpleEnumerator GetSources (in nsIRDFResource aProperty,
     *                                 in nsIRDFNode aTarget,
     *                                 in boolean aTruthValue);
     */
    GetSources: function(aProperty, aTarget, aTruthValue) {
        if (DEBUG) {
            dump("GetSources() called with args: \n\t" + aProperty.Value + 
             "\n\t" + aTarget.Value + "\n\t" + aTruthValue + "\n\n");
        }

        return new ArrayEnumerator(new Array());
    },

    /**
     * Find a child of that is related to the source by the given arc
     * arc and truth value
     *
     * @return NS_RDF_NO_VALUE if there is no target accessable from the
     * source via the specified property.
     *
     * nsIRDFNode GetTarget (in nsIRDFResource aSource,
     *                       in nsIRDFResource aProperty,
     *                       in boolean aTruthValue);
     */
    GetTarget: function(aSource, aProperty, aTruthValue) {
        if (DEBUG) {
            dump("GetTarget() called with args: \n\t" + aSource.Value + 
             "\n\t" + aProperty.Value + "\n\t" + aTruthValue + "\n\n");
        }

        var delegate;
        var enumerator;

        if (aProperty.EqualsNode(this.kNC_child)) {
            try {
                delegate = aSource.GetDelegate("messagelist.ldap",
                                       Components.interfaces.nsISupportsArray);
            } catch (e) {
            }
            if (delegate != null) {
                if (delegate.Count() == 1) {
                    return delegate.QueryElementAt(0,
                                             Components.interfaces.nsIRDFNode);
                }
            }
        }
        else {
            var refStart = aProperty.Value.indexOf("#");
            if ( aProperty.Value.slice(0, refStart + 1) == 
                 LDAPATTR_NAMESPACE_URI) {

                try {
                    delegate = aSource.GetDelegate("message.ldap",
                            Components.interfaces.nsILDAPMessage);
                } catch (e) {
                }
                if (delegate != null) {
                    var attributeName = aProperty.Value.slice(refStart + 1);
                    enumerator = new ArrayEnumerator(this.getAttributeArray(
                        delegate, attributeName));
                    if (enumerator.hasMoreElements()) {
                        return enumerator.getNext();
                    }
                }
            }
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
     * nsISimpleEnumerator GetTargets (in nsIRDFResource aSource,
     *                                 in nsIRDFResource aProperty,
     *                                 in boolean aTruthValue);
     */
    GetTargets: function(aSource, aProperty, aTruthValue) {
        if (DEBUG) {
            dump("GetTargets() called with args: \n\t" + aSource.Value + 
             "\n\t" + aProperty.Value + "\n\t" + aTruthValue + "\n\n");
        }
        
        var delegate;

        if (aProperty.EqualsNode(this.kNC_child)) {
            try {
                delegate = aSource.GetDelegate("messagelist.ldap",
                                       Components.interfaces.nsISupportsArray);
            } catch (e) {
            }
            if (delegate != null) {
                return new nsISupportsArrayEnumerator(delegate);
            }
        } else {
            var refStart = aProperty.Value.indexOf("#");
            if ( aProperty.Value.slice(0, refStart + 1) == 
                 LDAPATTR_NAMESPACE_URI) {
                try {
                    delegate = aSource.GetDelegate("message.ldap",
                            Components.interfaces.nsILDAPMessage);
                } catch (e) {
                }
                if (delegate != null) {
                    var attributeName = aProperty.Value.slice(refStart + 1);
                    return new ArrayEnumerator(
                        this.getAttributeArray(delegate, attributeName));
                }
            }
        }

        return new ArrayEnumerator(new Array());
    },

    /**
     * Add an assertion to the graph.
     *
     * void Assert (in nsIRDFResource aSource,
     *              in nsIRDFResource aProperty,
     *              in nsIRDFNode aTarget,
     *              in boolean aTruthValue);
     */
    Assert: function(aSource, aProperty, aTarget, aTruthValue) {
        if (DEBUG) {
            dump("Assert() called with args: \n\t" + aSource.Value + 
             "\n\t" + aProperty.Value + "\n\t" + aTarget.Value +
             "\n\t" + aTruthValue + "\n\n");
        }
    },

    /**
     * Remove an assertion from the graph.
     *
     * void Unassert (in nsIRDFResource aSource,
     *                in nsIRDFResource aProperty,
     *                in nsIRDFNode aTarget);
     */
    Unassert: function(aSource, aProperty, aTarget) {
        if (DEBUG) {
            dump("Unassert() called with args: \n\t" + aSource.Value + 
             "\n\t" + aProperty.Value + "\n\t" + aTarget.Value + "\n\n");
        }
    },

    /**
     * Change an assertion from
     *
     *   [aSource]--[aProperty]-->[aOldTarget]
     *
     * to
     * 
     *   [aSource]--[aProperty]-->[aNewTarget]
     *
     * void Change (in nsIRDFResource aSource,
     *              in nsIRDFResource aProperty,
     *              in nsIRDFNode aOldTarget,
     *              in nsIRDFNode aNewTarget);
     */
    Change: function(aSource, aProperty, aOldTarget, aNewTarget) {
        if (DEBUG) {
            dump("Change() called with args: \n\t" + aSource.Value + 
             "\n\t" + aProperty.Value + "\n\t" + aOldTarget.Value +
             "\n\t" + aNewTarget.Value + "\n\n");
        }
    },

    /**
     * 'Move' an assertion from
     *
     *   [aOldSource]--[aProperty]-->[aTarget]
     *
     * to
     * 
     *   [aNewSource]--[aProperty]-->[aTarget]
     *
     * void Move (in nsIRDFResource aOldSource,
     *            in nsIRDFResource aNewSource,
     *            in nsIRDFResource aProperty,
     *            in nsIRDFNode aTarget);
     */
    Move: function(aOldSource, aNewSource, aProperty, aTarget) {
        if (DEBUG) {
            dump("Move() called with args: \n\t" + aOldSource.Value + 
             "\n\t" + aNewSource.Value + "\n\t" + aProperty.Value +
             "\n\t" + aTarget.Value + "\n\n");
        }
    },

    /**
     * Query whether an assertion exists in this graph.
     *
     * boolean HasAssertion(in nsIRDFResource aSource,
     *                      in nsIRDFResource aProperty,
     *                      in nsIRDFNode     aTarget,
     *                      in boolean        aTruthValue);
     */
    HasAssertion: function(aSource, aProperty, aTarget, aTruthValue) {
        // the datasource doesn't currently use any sort of containers
        //
        if (aProperty.EqualsNode(this.kRDF_instanceOf)) {
            return false;
        }

        if (!aTruthValue) {
            return false;
        }

        var target = this.getSpecifiedNode(aTarget);

        // spew debugging info
        //
        if (DEBUG) {
            dump("HasAssertion() called with args: \n\t" + aSource.Value +
             "\n\t" + aProperty.Value + "\n\t" + target.Value + "\n\t" +
             aTruthValue + "\n\n");
        }

        var delegate;

        if (aProperty.EqualsNode(this.kNC_child)) {
            try {
               delegate = aSource.GetDelegate("messagelist.ldap",
                       Components.interfaces.nsISupportsArray);
            } catch (e) {
            }
            if (delegate != null) {
                var enumerator = delegate.Enumerate();
                var done = false;
                try {
                    enumerator.isDone();
                }
                catch (e) {
                    done = true;
                }
                while(!done) {
                    var resource = enumerator.currentItem().QueryInterface(
                            Components.interfaces.nsIRDFResource);
                    if (resource.Value == target.Value) {
                        return true;
                    }
                    try {
                        enumerator.next();
                    }
                    catch (e) {
                        done = true;
                    }
                }
            }
        }
        else {
            var refStart = aProperty.Value.indexOf("#");
            if (aProperty.Value.slice(0, refStart + 1) == 
                LDAPATTR_NAMESPACE_URI) {
                try {
                    delegate = aSource.GetDelegate("message.ldap",
                            Components.interfaces.nsILDAPMessage);
                } catch (e) {
                }
                if (delegate != null) {
                    var attributeName = aProperty.Value.slice(refStart + 1);
                    var attributeArray = this.getAttributeArray(delegate,
                                                                attributeName);
                    var attributes = new ArrayEnumerator(attributeArray);
                    while (attributes.hasMoreElements()) {
                        var attribute = attributes.getNext();
                        if (attribute.Value == target.Value) {
                            return true;
                        }
                    }
                }
            }
        }

        // if we haven't returned true yet, there's nothing here
        //
        return false;
    },

    /**
     * Add an observer to this data source. If the datasource
     * supports observers, the datasource source should hold a strong
     * reference to the observer.
     *
     * void AddObserver(in nsIRDFObserver aObserver);
     */
    AddObserver: function(aObserver) {
        if (DEBUG) {
            dump("AddObserver() called\n\n");
        }

        var eventQSvc = Components.
                classes["@mozilla.org/event-queue-service;1"].
                getService(Components.interfaces.nsIEventQueueService);
        var uiQueue = eventQSvc.
                getSpecialEventQueue(Components.interfaces.
                nsIEventQueueService.UI_THREAD_EVENT_QUEUE);
        var proxyMgr = Components.
                classes["@mozilla.org/xpcomproxy;1"].
                getService(Components.interfaces.nsIProxyObjectManager);

        var observer = proxyMgr.getProxyForObject(uiQueue, 
                Components.interfaces.nsIRDFObserver, aObserver, 5); 
        // 5 == PROXY_ALWAYS | PROXY_SYNC

        this.mObserverList.push(observer);
    },

    /**
     * Remove an observer from this data source.
     *
     * void RemoveObserver (in nsIRDFObserver aObserver);
     */
    RemoveObserver: function(aObserver) {
        if (DEBUG) {
            dump("RemoveObserver() called" + "\n\n");
        }

        var iter = new ArrayEnumerator(this.mObserverList);
        var nextObserver;
        while ((nextObserver = iter.getNext())) {
            if (nextObserver == aObserver) {
                splice(iter.index, 1);
            }
        }
    },

    /**
     * Get a cursor to iterate over all the arcs that point into a node.
     *
     * @return NS_OK unless a catastrophic error occurs. If the method
     * returns NS_OK, you may assume that labels points to a valid (but
     * possible empty) nsISimpleEnumerator object.
     *
     * nsISimpleEnumerator ArcLabelsIn (in nsIRDFNode aNode);
     */
    ArcLabelsIn: function(aNode) {
        if (DEBUG) {
            dump("ArcLabelsIn() called with args: \n\t" + aNode.Value + 
             "\n\n");
        }

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
    },

    /**
     * Returns true if the specified node is pointed to by the specified arc.
     * Equivalent to enumerating ArcLabelsIn and comparing for the specified
     * arc.
     *
     * boolean hasArcIn (in nsIRDFNode aNode,
     *                   in nsIRDFResource aArc);
     */
    hasArcIn: function(aNode, aArc) 
    {
        if (DEBUG) {
            dump("hasArcIn() called with args: \n\t" + aNode.Value + 
             "\n\t" + aArc.Value + "\n\n");
        }

        return false;
    },

    /**
     * Returns true if the specified node has the specified outward arc.
     * Equivalent to enumerating ArcLabelsOut and comparing for the specified 
     * arc.
     *
     * boolean hasArcOut (in nsIRDFResource aSource,
     *                    in nsIRDFResource aArc);
     */
    hasArcOut: function(aSource, aArc) 
    {
        if (DEBUG) {
            dump("hasArcOut() called with args: \n\t" + aSource.Value + 
             "\n\t" + aArc.Value + "\n\n");
        }

        var delegate;

        if (aArc.EqualsNode(this.kNC_child)) {
            try {
                delegate = aSource.GetDelegate("messagelist.ldap",
                        Components.interfaces.nsISupportsArray);
            } catch (e) {
            }
            if (delegate != null) {
                return true;
            }
        }
        else {
            var refStart = aArc.Value.indexOf("#");
            if (aArc.Value.slice(0, refStart + 1) == LDAPATTR_NAMESPACE_URI) {
                try {
                    delegate = aSource.GetDelegate("message.ldap",
                            Components.interfaces.nsILDAPMessage);
                } catch (e) {
                }
                if (delegate != null) {
                    var attributeName = aArc.Value.slice(refStart + 1);
                    var attributeArray = this.getAttributeArray(delegate, 
                                                                attributeName);
                    if (attributeArray.length > 0) {
                        return true;
                    }
                }
            }
        }

        return false;
    },

    /**
     * nsIRDFRemoteDataSource
     */

    // from nsIRDFRemoteDataSource.  right now this is just a dump()
    // to see if it ever even gets used.
    //
    get loaded() {
        dump("getter for loaded called\n"); 
        return false;
    },

    /**
     * Private functions
     */

    onAssert: function(aDataSource, aSource, aProperty, aTarget)
    {
        if (DEBUG) {
            var target = this.getSpecifiedNode(aTarget);
            dump("OnAssert() called with args: \n\t" + aSource.Value + 
             "\n\t" + aProperty.Value + "\n\t" + target.Value + "\n\n");
        }

        var iter = new ArrayEnumerator(this.mObserverList);
        var nextObserver;
        while ((nextObserver = iter.getNext()) != null) {
            nextObserver.onAssert(this, aSource, aProperty, aTarget);
        }
    },

    beginUpdateBatch: function()
    {
        if (DEBUG) {
            dump("BeginUpdateBatch() called\n\n");
        }

        var iter = new ArrayEnumerator(this.mObserverList);
        var nextObserver;
        while ((nextObserver = iter.getNext()) != null) {
            nextObserver.beginUpdateBatch(this);
        }
    },

    endUpdateBatch: function()
    {
        if (DEBUG) {
            dump("EndUpdateBatch() called\n\n");
        }

        var iter = new ArrayEnumerator(this.mObserverList);
        var nextObserver;
        while ((nextObserver = iter.getNext()) != null) {
            nextObserver.endUpdateBatch(this);
        }
    },

    getSpecifiedNode: function(aNode) {
        // figure out what kind of nsIRDFNode aTarget is
        //
        var node;
        try {
            node = aNode.QueryInterface(Components.interfaces.nsIRDFResource);
        }
        catch (e) {
        }
        if (node == null) {
            try {
                node = aNode.QueryInterface(
                    Components.interfaces.nsIRDFLiteral);
            }
            catch (e) {
            }
        }
        if (node == null) {
            try {
                node = aNode.QueryInterface(Components.interfaces.nsIRDFDate);
            }
            catch (e) {
            }
        }
        if (node == null) {
            try {
                node = aNode.QueryInterface(Components.interfaces.nsIRDFInt);
            }
            catch (e) {
            }
        }
        return node;
    },
    
    getAttributeArray: function(aMessage, aAttributeName) {
        var resultArray = new Array();
        var attributesCount = {};
        var attributes = aMessage.getAttributes(attributesCount);
        for (var i = 0; i < attributesCount.value; i++) {
            if (attributes[i] == aAttributeName) {
                var valuesCount = {};
                var values =  aMessage.getValues(attributes[i], valuesCount);
                for (var j = 0; j < valuesCount.value; j++) {
                    var attributeValue = this.mRdfSvc.GetLiteral(values[j]);
                    resultArray.push(attributeValue);
                }
            }
        }
        return resultArray;
    }
}

// the nsILDAPMessage associated with a given resource
//
const NS_LDAPMESSAGERDFDELEGATEFACTORY_CONTRACTID = 
    '@mozilla.org/rdf/delegate-factory;1?key=message.ldap&scheme=ldap'
const NS_LDAPMESSAGELISTRDFDELEGATEFACTORY_CONTRACTID = 
    '@mozilla.org/rdf/delegate-factory;1?key=messagelist.ldap&scheme=ldap'
const NS_LDAPMESSAGERDFDELEGATEFACTORY_CID = 
    Components.ID('{4b6fb566-1dd2-11b2-a1a9-889a3f852b0b}');

function nsLDAPMessageRDFDelegateFactory() {}

nsLDAPMessageRDFDelegateFactory.prototype = 
{
    mRdfSvc: {},
    mLDAPDataSource: {},
    kNC_child: {},

    mConnection: {},        // connection to the LDAP server
    mOperation: {},         // current LDAP operation
    mMessagesHash: {},      // hash with the currently known messages
                            // (hashed on URI)
    mMessagesListHash: {},
    mInProgressHash: {},
    kInited: -1,

    Init: function() {
        if (this.kInited == -1) {
            this.kInited = 0;

            // get the RDF service
            //
            this.mRdfSvc = Components.
                    classes["@mozilla.org/rdf/rdf-service;1"].
                    getService(Components.interfaces.nsIRDFService);
            this.mLDAPDataSource = this.mRdfSvc.GetDataSource("rdf:ldap").
                    QueryInterface(Components.interfaces.nsIRDFObserver);

            // get some RDF Resources that we'll need
            //
            this.kNC_child = this.mRdfSvc.GetResource( NC_NAMESPACE_URI + 
                                                       "child");
        }
    },

    QueryInterface: function(iid) {
        if (DEBUG) {
            dump("QueryInterface called\n\n");
        }
        if (!iid.equals(Components.interfaces.nsISupports) &&
            !iid.equals(Components.interfaces.nsIRDFDelegateFactory))
            throw Components.results.NS_ERROR_NO_INTERFACE;

        return this;
    },

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
        
        function generateGetTargetsBoundCallback() {
            function getTargetsBoundCallback() {
            }

            getTargetsBoundCallback.prototype.QueryInterface =
                function(iid) {
                    if (!iid.equals(Components.interfaces.nsISupports) &&
                        !iid.equals(Components.interfaces.nsILDAPMessageListener))
                        throw Components.results.NS_ERROR_NO_INTERFACE;

                    return this;
                }

            getTargetsBoundCallback.prototype.onLDAPMessage = 
                function(aMessage) {
                    if (DEBUG) {
                        dump("getTargetsBoundCallback.onLDAPMessage()" +
                             "called with scope: \n\t" +
                             aOuter.Value + "\n\n");
                    }

                    // XXX how do we deal with this in release builds?
                    // XXX deal with already bound case
                    //
                    if (aMessage.type != aMessage.RES_BIND) {
                        dump("bind failed\n");
                        if (aKey == "messagelist.ldap") {
                            delete callerObject.mInProgressHash[queryURL.spec];
                        }
                        return;
                    }

                    // kick off a search
                    //
                    var searchOp = Components.classes[
                            "@mozilla.org/network/ldap-operation;1"].
                            createInstance(Components.interfaces.
                            nsILDAPOperation);

                    // XXX err handling
                    searchOp.init(connection,
                                  generateGetTargetsSearchCallback());
                    // XXX err handling (also for url. accessors)
                    // XXX constipate this
                    // XXX real timeout
                    searchOp.searchExt(url.dn, url.scope, url.filter, 0,
                                       new Array(), 0, -1);
                }

            getTargetsBoundCallback.prototype.onLDAPInit = 
                function(aStatus) {
                    if (DEBUG) {
                        dump("getTargetsBoundCallback.onLDAPInit()" +
                             " called with status of " + aStatus + "\n\n");
                    }

                    // get and initialize an operation object
                    //
                    var operation = Components.classes
                           ["@mozilla.org/network/ldap-operation;1"].
                           createInstance(Components.interfaces.nsILDAPOperation);
                    operation.init(connection,
                                   getProxyOnUIThread(this, Components.interfaces.
                                                            nsILDAPMessageListener));

                    caller.mInProgressHash[aOuter.Value] = 1;

                    // bind to the server.  we'll get a callback when this
                    // finishes. XXX handle a password
                    //
                    operation.simpleBind(null);
                }

            return getProxyOnUIThread(new getTargetsBoundCallback(),
                                      Components.interfaces.nsILDAPMessageListener);
        }

        function generateGetTargetsSearchCallback() {
            function getTargetsSearchCallback() {
            }

            getTargetsSearchCallback.prototype.QueryInterface = function(iid) {
                if (!iid.equals(Components.interfaces.nsISupports) &&
                    !iid.equals(Components.interfaces.nsILDAPMessageListener))
                    throw Components.results.NS_ERROR_NO_INTERFACE;

                return this;
            };

            getTargetsSearchCallback.prototype.onLDAPMessage = 
                function(aMessage) {
                    if (DEBUG) {
                        dump("getTargetsSearchCallback() called with message" +
                               " of type " + aMessage.type + "\n\n");
                    }

                    if (aMessage.type == aMessage.RES_SEARCH_ENTRY) {
                        if (DEBUG) {
                            dump("getTargetsSearchCallback() called with " + 
                                 "message " + aMessage.dn + " for " + 
                                 aOuter.Value + "\n\n");
                        }

                        // XXX (pvdb) Should get this out of nsILDAPMessage
                        var newURL = "ldap://" + url.host;
                        newURL += (url.port == "" ? "" : ":" + url.port);
                        newURL += ("/" + aMessage.dn + "??base");
                        var newResource = 
                            caller.mRdfSvc.GetResource(newURL);

                        // Add the LDAP message for the new resource in our
                        // messages hashlist. We do this before adding it to
                        // a query result array (when applicable), so that
                        // when we onAssert this as a result for a query
                        // returning multiple results, it is already cached.
                        // If we wouldn't do this here, we'd start another
                        // query for this resource.
                        var messageHash = caller.mMessagesHash;
                        if (!messageHash.hasOwnProperty(newURL)) {
                            messageHash[newURL] = aMessage;
                        }

                        if (aKey == "messagelist.ldap") {
                            // Add the LDAP message to the result array for
                            // the query resource and onAssert the result
                            // as a child of the query resource.
                            var listHash = caller.mMessagesListHash;
                            if (!listHash.hasOwnProperty(aOuter.Value)) {
                                // No entry for the query resource in the
                                // results hashlist, let's create one.
                                listHash[aOuter.Value] = Components.classes
                                    ["@mozilla.org/supports-array;1"].
                                    createInstance(
                                       Components.interfaces.nsISupportsArray);
                            }
                            listHash[aOuter.Value].AppendElement(
                                newResource);
                            caller.mLDAPDataSource.onAssert(
                                caller.mLDAPDataSource, aOuter,
                                caller.kNC_child, newResource);
                        } else if (aKey == "message.ldap") {
                            // XXX - we need to onAssert this resource. However,
                            // we need to know somehow what the caller wanted
                            // to use this delegate for, so that we can limit
                            // the onAssert to what was asked for. This might be
                            // tricky, as we'd need to keep track of multiple
                            // "questions" coming in while the LDAP server
                            // hasn't responded yet.
                        }
                    }
                    else if (aMessage.type == aMessage.RES_SEARCH_RESULT) {
                        delete caller.mInProgressHash[aOuter.Value];
                    }
                }

            return getProxyOnUIThread(new getTargetsSearchCallback(),
                                      Components.interfaces.nsILDAPMessageListener);
        }

        // end of closure decls; the CreateDelegate code proper begins below

        // XXX - We need to keep track of queries that are in progress for a
        // message too (cf. mInProgressHash for messagelist's).
        //
        if (DEBUG) {
            dump("GetDelegate() called with args: \n\t" + aOuter.Value + 
             "\n\t" + aKey + "\n\t" + aIID + "\n\n");
        }

        var caller = this;

        if (aKey == "messagelist.ldap") {
            if (this.mMessagesListHash.hasOwnProperty(aOuter.Value)) {
                return (this.mMessagesListHash[aOuter.Value].
                            QueryInterface(aIID));
            }
        } else if (aKey == "message.ldap") {
            if (this.mMessagesHash.hasOwnProperty(aOuter.Value)) {
                return (this.mMessagesHash[aOuter.Value].QueryInterface(aIID));
            }
        }

        if (!((aKey == "messagelist.ldap") &&
              (this.mInProgressHash.hasOwnProperty(aOuter.Value)))) {
            this.Init();
            var url = Components.classes["@mozilla.org/network/ldap-url;1"]
                      .createInstance(Components.interfaces.nsILDAPURL);
            url.spec = aOuter.Value;

            // make sure that this if this URL is for a messagelist, it 
            // represents something other than a base search
            //
            if ((aKey == "messagelist.ldap") && (url.scope == url.SCOPE_BASE)) {
                throw Components.results.NS_ERROR_FAILURE;
            }

            // get a connection object
            //
            var connection = Components.classes
                    ["@mozilla.org/network/ldap-connection;1"].
                    createInstance(Components.interfaces.nsILDAPConnection);
            connection.init(url.host, url.port, null,
                            generateGetTargetsBoundCallback())

            // XXXdmose - in this case, we almost certainly shouldn't be
            // falling through to an error case, but instead returning 
            // something reasonable, since this is actually a success 
            // condition
            //
            dump ("falling through!\n");
        }

        throw Components.results.NS_ERROR_FAILURE;
    }
    
}

// the nsILDAPURL associated with a given resource
//
const NS_LDAPURLRDFDELEGATEFACTORY_CONTRACTID =
    '@mozilla.org/rdf/delegate-factory/url.ldap;1';
const NS_LDAPURLRDFDELEGATEFACTORY_CID = 
    Components.ID('b6048700-1dd1-11b2-ae88-a5e18bb1f25e');

function nsLDAPURLRDFDelegateFactory() {}

nsLDAPURLRDFDelegateFactory.prototype = 
{
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

// the nsILDAPConnection associated with a given resource
//
const NS_LDAPCONNECTIONRDFDELEGATEFACTORY_CONTRACTID = 
    '@mozilla.org/rdf/delegate-factory/connection.ldap;1';
const NS_LDAPCONNECTIONRDFDELEGATEFACTORY_CID = 
    Components.ID('57075fc6-1dd2-11b2-9df5-dbb9111d1b38');

function nsLDAPConnectionRDFDelegateFactory() {}

nsLDAPConnectionRDFDelegateFactory.prototype = 
{
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

var Datasource = null;
var MessageDelegateFactory = null;

// nsLDAPDataSource Module (for XPCOM registration)
// 
var nsLDAPDataSourceModule = {

    registerSelf: function (compMgr, fileSpec, location, type) {
        if (DEBUG) {
            dump("*** Registering LDAP datasource components" +
                 " (all right -- a JavaScript module!)\n");
        }
        compMgr.registerComponentWithType(
                NS_LDAPDATASOURCE_CID, 
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

        compMgr.registerComponentWithType(
                NS_LDAPMESSAGERDFDELEGATEFACTORY_CID, 
                'LDAP MessageList RDF Delegate', 
                NS_LDAPMESSAGELISTRDFDELEGATEFACTORY_CONTRACTID, 
                fileSpec, location, true, true, 
                type);

        compMgr.registerComponentWithType(
                NS_LDAPURLRDFDELEGATEFACTORY_CID, 
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
        if (!iid.equals(Components.interfaces.nsIFactory)) {
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
        }

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
      
            if (Datasource == null) {
                Datasource = new nsLDAPDataSource();
            }

            return Datasource.QueryInterface(iid);
        }
    },

    nsLDAPMessageRDFDelegateFactoryFactory: {
        createInstance: function(outer, iid) {
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;

            if (MessageDelegateFactory == null) {
                MessageDelegateFactory = new nsLDAPMessageRDFDelegateFactory();
            }
      
            return MessageDelegateFactory.QueryInterface(iid);
        }
    },

    nsLDAPURLRDFDelegateFactoryFactory: {
        createInstance: function(outer, iid) {
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;
      
            return (new nsLDAPURLRDFDelegateFactory()).QueryInterface(iid);
        }
    },

    // probably don't need this; use nsLDAPService instead
    //
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
