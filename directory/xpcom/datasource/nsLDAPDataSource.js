/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the mozilla.org LDAP RDF datasource.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dan Mosedale <dmose@mozilla.org>
 *   Brendan Eich <brendan@mozilla.org>
 *   Peter Van der Beken <peter.vanderbeken@pandora.be>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

// Scope constants
//
const SCOPE_BASE = Components.interfaces.nsILDAPURL.SCOPE_BASE;
const SCOPE_ONELEVEL = Components.interfaces.nsILDAPURL.SCOPE_ONELEVEL;
const SCOPE_SUBTREE = Components.interfaces.nsILDAPURL.SCOPE_SUBTREE;

// Types of delegates
const MESSAGE = 1;
const FLAT_LIST = 2;
const RECURSIVE_LIST = 3;

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
    kNC_DN: {},
    kNC_child: {},
    kNC_recursiveChild: {},

    mRdfSvc: {},

    // since we implement multiple interfaces, we have to provide QI
    //
    QueryInterface: function(iid) {
      if (iid.equals(Components.interfaces.nsISupports) ||
          iid.equals(Components.interfaces.nsIRDFDataSource) ||
          iid.equals(Components.interfaces.nsIRDFRemoteDataSource) ||
          iid.equals(Components.interfaces.nsIRDFObserver))
          return this;

      Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
      return null;
    },

    /**
     * nsIRDFRemoteDataSource
     */

    /**
     * This value is <code>true</code> when the datasource has
     * fully loaded itself.
     */
    get loaded() {
        debug("getter for loaded called\n");
        // We're never fully loaded
        return false;
    },

    /**
     * Specify the URI for the data source: this is the prefix
     * that will be used to register the data source in the
     * data source registry.
     * @param aURI the URI to load
     */
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
        this.kRDF_instanceOf = this.mRdfSvc.GetResource(RDF_NAMESPACE_URI +
                                                        "instanceOf");
        this.kNC_DN = this.mRdfSvc.GetResource(NC_NAMESPACE_URI + "DN");
        this.kNC_child = this.mRdfSvc.GetResource(NC_NAMESPACE_URI + "child");
        this.kNC_recursiveChild = this.mRdfSvc.GetResource(NC_NAMESPACE_URI +
                                                           "recursiveChild");

        return;
    },

    /**
     * Refresh the remote datasource, re-loading its contents
     * from the URI.
     *
     * @param aBlocking If <code>true</code>, the call will block
     * until the datasource has completely reloaded.
     */
    Refresh: function(aBlocking)
    {
        if (DEBUG) {
            dump("Refresh() called with args: \n\t" + aBlocking + "\n\n");
        }
        return;
    },

    /**
     * Request that a data source write it's contents out to
     * permanent storage, if applicable.
     */
    Flush: function()
    {
        if (DEBUG) {
            dump("Flush() called\n\n");
        }
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
     * and truth value
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

        // We don't handle negative assertions.
        // XXX Should we? Can we?
        if (!aTruthValue) {
            Components.returnCode = NS_RDF_NO_VALUE;
            return null;
        }

        var delegate;
        var enumerator;

        if (aProperty.EqualsNode(this.kNC_child)
            || aProperty.EqualsNode(this.kNC_recursiveChild)) {
            // Find a child or recursiveChild. Get the messagelist delegate
            // (flat for child, recursive for recursiveChild) for aSource and
            // return the first element of the list, if available.
            var listType = (aProperty.EqualsNode(this.kNC_child) ?
                            "flat.messagelist.ldap" :
                            "recursive.messagelist.ldap");
            try {
                delegate = aSource.GetDelegate(listType,
                                       Components.interfaces.nsISupportsArray);
            }
            catch (e) {
            }
            if (delegate != null) {
                try {
                    var target = delegate.QueryElementAt(0,
                                       Components.interfaces.nsIRDFNode);
                    return target;
                }
                catch (e) {
                }
            }
        }
        else if (aProperty.EqualsNode(this.kNC_DN)) {
            // Find the DN arc. Get the message delegate for aSource
            // and return the resource for the dn property of the message,
            // if available.
            try {
                delegate = aSource.GetDelegate("message.ldap",
                                       Components.interfaces.nsILDAPMessage);
            }
            catch (e) {
            }
            if (delegate != null) {
                return this.mRdfSvc.GetResource(delegate.dn);
            }
        }
        else {
            // Find a different arc. See if we're looking for an LDAP attribute
            // arc. If so, get the message delegate for aSource, if we find one
            // get the attribute array for the specified LDAP attribute and
            // return the first value if it isn't empty.
            var refStart = aProperty.Value.indexOf("#");
            if (aProperty.Value.slice(0, refStart + 1) ==
                LDAPATTR_NAMESPACE_URI) {

                try {
                    delegate = aSource.GetDelegate("message.ldap",
                            Components.interfaces.nsILDAPMessage);
                }
                catch (e) {
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

        // Found nothing.
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

        // We don't handle negative assertions.
        // XXX Should we? Can we?
        if (!aTruthValue) {
            return new ArrayEnumerator(new Array());
        }

        var delegate;

        if (aProperty.EqualsNode(this.kNC_child)
            || aProperty.EqualsNode(this.kNC_recursiveChild)) {
            // Find children or recursiveChildren. Get the messagelist delegate
            // (flat for child, recursive for recursiveChild) for aSource and
            // return an enumerator for the list, if available.
            var listType = (aProperty.EqualsNode(this.kNC_child) ?
                            "flat.messagelist.ldap" :
                            "recursive.messagelist.ldap");
            try {
                delegate = aSource.GetDelegate(listType,
                                       Components.interfaces.nsISupportsArray);
            }
            catch (e) {
            }
            if (delegate != null) {
                return new nsISupportsArrayEnumerator(delegate);
            }
        }
        else if (aProperty.EqualsNode(this.kNC_DN)) {
            // Find the DN arc. Get the message delegate for aSource
            // and return the resource for the dn property of the message,
            // if available.
            try {
                delegate = aSource.GetDelegate("message.ldap",
                                       Components.interfaces.nsILDAPMessage);
            }
            catch (e) {
            }
            if (delegate != null) {
                return this.mRdfSvc.GetResource(delegate.dn);
            }
        }
        else {
            // Find a different arc. See if we're looking for an LDAP attribute
            // arc. If so, get the message delegate for aSource, if we find one
            // get the attribute array for the specified LDAP attribute and
            // an enumerator for the array.
            var refStart = aProperty.Value.indexOf("#");
            if (aProperty.Value.slice(0, refStart + 1) ==
                 LDAPATTR_NAMESPACE_URI) {
                try {
                    delegate = aSource.GetDelegate("message.ldap",
                                       Components.interfaces.nsILDAPMessage);
                }
                catch (e) {
                }
                if (delegate != null) {
                    var attributeName = aProperty.Value.slice(refStart + 1);
                    return new ArrayEnumerator(
                        this.getAttributeArray(delegate, attributeName));
                }
            }
        }

        // Found nothing, return enumerator for empty array.
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
        // The datasource doesn't currently use any sort of standard
        // RDF containers.
        if (aProperty.EqualsNode(this.kRDF_instanceOf)) {
            return false;
        }

        if (DEBUG) {
            dump("HasAssertion() called with args: \n\t" + aSource.Value +
             "\n\t" + aProperty.Value + "\n\t" + aTarget.Value + "\n\t" +
             aTruthValue + "\n\n");
        }

        // We don't handle negative assertions.
        // XXX Should we? Can we?
        if (!aTruthValue) {
            return false;
        }

        var delegate;

        if (aProperty.EqualsNode(this.kNC_child)
            || aProperty.EqualsNode(this.kNC_recursiveChild)) {
            // Find out if aTarget is in the children or recursiveChildren of
            // aSource. Get the messagelist delegate (flat for child, recursive
            // for recursiveChild) for aSource.
            var listType = (aProperty.EqualsNode(this.kNC_child) ?
                            "flat.messagelist.ldap" :
                            "recursive.messagelist.ldap");
            try {
                delegate = aSource.GetDelegate(listType,
                                       Components.interfaces.nsISupportsArray);
            }
            catch (e) {
            }
            if (delegate != null) {
                // We have a delegate message list, loop through it and return
                // true if the target is in the list.
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
                    if (resource.Value == aTarget.Value) {
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
        else if (aProperty.EqualsNode(this.kNC_DN)) {
            // Find the DN arc. Get the message delegate for aSource
            // and return the resource for the dn property of the message,
            // if available.
            try {
                delegate = aSource.GetDelegate("message.ldap",
                                       Components.interfaces.nsILDAPMessage);
            }
            catch (e) {
            }
            if (delegate != null) {
                return (delegate.dn == aTarget.Value);
            }
        }
        else {
            // Find a different arc. See if we're looking for an LDAP attribute
            // arc. If so, get the message delegate for aSource, if we find one
            // get the attribute array for the specified LDAP attribute and
            // an enumerator for the array.
            var refStart = aProperty.Value.indexOf("#");
            if (aProperty.Value.slice(0, refStart + 1) ==
                LDAPATTR_NAMESPACE_URI) {
                try {
                    delegate = aSource.GetDelegate("message.ldap",
                            Components.interfaces.nsILDAPMessage);
                }
                catch (e) {
                }
                if (delegate != null) {
                    var attributeName = aProperty.Value.slice(refStart + 1);
                    var attributeArray = this.getAttributeArray(delegate,
                                                                attributeName);
                    var attributes = new ArrayEnumerator(attributeArray);
                    while (attributes.hasMoreElements()) {
                        var attribute = attributes.getNext();
                        if (attribute.Value == aTarget.Value) {
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

        // We need to proxy our observers on the main (UI) thread, because
        // the RDF code wants to run on the main thread.
        this.mObserverList.push(getProxyOnUIThread(aObserver,
                Components.interfaces.nsIRDFObserver));
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

        if (aArc.EqualsNode(this.kNC_child)
            || aArc.EqualsNode(this.kNC_recursiveChild)) {
            // Find children or recursiveChildren. Get the messagelist delegate
            // (flat for child, recursive for recursiveChild) for aSource and
            // return true if we have one.
            var listType = (aArc.EqualsNode(this.kNC_child) ?
                            "flat.messagelist.ldap" :
                            "recursive.messagelist.ldap");
            try {
                delegate = aSource.GetDelegate(listType,
                       Components.interfaces.nsISupportsArray);
            }
            catch (e) {
            }
            if (delegate != null) {
                return true;
            }
        }
        else if (aArc.EqualsNode(this.kNC_DN)) {
            // Find the DN arc. Get the message delegate for aSource
            // and return true if we have a delegate and its dn
            // attribute is non-null.
            try {
                delegate = aSource.GetDelegate("message.ldap",
                        Components.interfaces.nsILDAPMessage);
            }
            catch (e) {
            }
            return (delegate != null);
        }

        // Find a different arc. See if we're looking for an LDAP attribute
        // arc. If so, get the message delegate for aSource, if we find one
        // get the attribute array for the specified LDAP attribute and
        // return true if it contains at least one value.
        var refStart = aArc.Value.indexOf("#");
        if (aArc.Value.slice(0, refStart + 1) == LDAPATTR_NAMESPACE_URI) {
            try {
                delegate = aSource.GetDelegate("message.ldap",
                        Components.interfaces.nsILDAPMessage);
            }
            catch (e) {
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
        return false;
    },

    /**
     * Private functions
     */

    onAssert: function(aDataSource, aSource, aProperty, aTarget)
    {
        if (DEBUG) {
            dump("OnAssert() called with args: \n\t" + aSource.Value +
             "\n\t" + aProperty.Value + "\n\t" + aTarget.Value + "\n\n");
        }

        var iter = new ArrayEnumerator(this.mObserverList);
        var nextObserver;
        while ((nextObserver = iter.getNext()) != null) {
            nextObserver.onAssert(this, aSource, aProperty, aTarget);
        }
    },

    onUnassert: function(aDataSource, aSource, aProperty, aTarget)
    {
        if (DEBUG) {
            dump("onUnassert() called with args: \n\t" + aSource.Value +
             "\n\t" + aProperty.Value + "\n\t" + aTarget.Value + "\n\n");
        }

        var iter = new ArrayEnumerator(this.mObserverList);
        var nextObserver;
        while ((nextObserver = iter.getNext()) != null) {
            nextObserver.onUnassert(this, aSource, aProperty, aTarget);
        }
    },

    onChange: function(aDataSource, aSource, aProperty, aOldTarget, aNewTarget)
    {
        if (DEBUG) {
            dump("onChange() called with args: \n\t" + aSource.Value +
             "\n\t" + aProperty.Value + "\n\t" + aOldTarget.Value +
             "\n\t" + aNewTarget.Value + "\n\n");
        }

        var iter = new ArrayEnumerator(this.mObserverList);
        var nextObserver;
        while ((nextObserver = iter.getNext()) != null) {
            nextObserver.onChange(this, aSource, aProperty, aOldTarget, aNewTarget);
        }
    },

    onBeginUpdateBatch: function()
    {
        if (DEBUG) {
            dump("onBeginUpdateBatch() called\n\n");
        }

        var iter = new ArrayEnumerator(this.mObserverList);
        var nextObserver;
        while ((nextObserver = iter.getNext()) != null) {
            nextObserver.onBeginUpdateBatch(this);
        }
    },

    onEndUpdateBatch: function()
    {
        if (DEBUG) {
            dump("onEndUpdateBatch() called\n\n");
        }

        var iter = new ArrayEnumerator(this.mObserverList);
        var nextObserver;
        while ((nextObserver = iter.getNext()) != null) {
            nextObserver.onEndUpdateBatch(this);
        }
    },

    /**
     * Fills an array with the RDF nodes for the values of an attribute
     * out of a nsLDAPMessage.
     */
    getAttributeArray: function(aMessage, aAttributeName) {
        var resultArray = new Array();
        var valuesCount = {};
        try {
            var values =  aMessage.getValues(aAttributeName, valuesCount);
            for (var j = 0; j < valuesCount.value; j++) {
                var attributeValue = this.mRdfSvc.GetLiteral(values[j]);
                resultArray.push(attributeValue);
            }
        }
        catch (e) {
            // Error or the attribute was not there.
            // XXX Do we need to do something about the real errors?
            // XXX Just returning empty array for now.
        }
        return resultArray;
    }
}

// the nsILDAPMessage associated with a given resource
//
const NS_LDAPMESSAGERDFDELEGATEFACTORY_CONTRACTID =
    '@mozilla.org/rdf/delegate-factory;1?key='+"message.ldap"+'&scheme=ldap'
const NS_LDAPFLATMESSAGELISTRDFDELEGATEFACTORY_CONTRACTID =
    '@mozilla.org/rdf/delegate-factory;1?key=flat.messagelist.ldap&scheme=ldap'
const NS_LDAPRECURSIVEMESSAGELISTRDFDELEGATEFACTORY_CONTRACTID =
    '@mozilla.org/rdf/delegate-factory;1?key=recursive.messagelist.ldap&scheme=ldap'
const NS_LDAPMESSAGERDFDELEGATEFACTORY_CID =
    Components.ID('{4b6fb566-1dd2-11b2-a1a9-889a3f852b0b}');

function nsLDAPMessageRDFDelegateFactory() {}

nsLDAPMessageRDFDelegateFactory.prototype =
{
    mRdfSvc: {},
    mLDAPSvc: {},
    mLDAPDataSource: {},
    kNC_child: {},
    kNC_recursiveChild: {},

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
            this.mLDAPSvc = Components.
                    classes["@mozilla.org/network/ldap-service;1"].
                    getService(Components.interfaces.nsILDAPService);
            this.mLDAPDataSource = this.mRdfSvc.GetDataSource("rdf:ldap").
                    QueryInterface(Components.interfaces.nsIRDFObserver);

            // get some RDF Resources that we'll need
            //
            this.kNC_child = this.mRdfSvc.GetResource(NC_NAMESPACE_URI +
                                                       "child");
            this.kNC_recursiveChild = this.mRdfSvc.GetResource(NC_NAMESPACE_URI +
                                                       "recursiveChild");
        }
    },

    QueryInterface: function(iid) {
        if (DEBUG) {
            dump("QueryInterface called\n\n");
        }
        if (iid.equals(Components.interfaces.nsISupports) ||
            iid.equals(Components.interfaces.nsIRDFDelegateFactory))
            return this;

        Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
        return null;
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
                    if (iid.equals(Components.interfaces.nsISupports) ||
                        iid.equals(Components.interfaces.nsILDAPMessageListener))
                        return this;

                    Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
                    return null;
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
                        delete callerObject.mInProgressHash[queryURL.spec];
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
                    searchOp.searchExt(queryURL.dn, queryURL.scope,
                                       queryURL.filter, 0, new Array(),
                                       0, -1);
                }

            getTargetsBoundCallback.prototype.onLDAPInit =
                function(aConn, aStatus) {
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
                if (iid.equals(Components.interfaces.nsISupports) ||
                    iid.equals(Components.interfaces.nsILDAPMessageListener))
                    return this;

                Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
                return null;
            };

            getTargetsSearchCallback.prototype.onLDAPMessage =
                function(aMessage) {
                    if (DEBUG) {
                        dump("getTargetsSearchCallback() called with message" +
                               " of type " + aMessage.type + "\n\n");
                    }

                    var listHash = caller.mMessagesListHash;
                    if (aMessage.type == aMessage.RES_SEARCH_ENTRY) {
                        if (DEBUG) {
                            dump("getTargetsSearchCallback() called with " +
                                 "message " + aMessage.dn + " for " +
                                 aOuter.Value + "\n\n");
                        }

                        // Create a resource for the message we just got.
                        // This is a single entry for which we'll cache the
                        // nsILDAPMessage. If we got this as a result of a
                        // "list" query we'll also add it to the result array.
                        var newURL = Components.classes["@mozilla.org/network/ldap-url;1"]
                              .createInstance(Components.interfaces.nsILDAPURL);
                        newURL.spec = queryURL.spec;
                        newURL.dn = aMessage.dn;
                        newURL.scope = SCOPE_BASE;
                        newURL.filter = "(objectClass=*)";
                        var newResource =
                            caller.mRdfSvc.GetResource(newURL.spec);

                        // Add the LDAP message for the new resource in our
                        // messages hashlist. We do this before adding it to
                        // a query result array (when applicable), so that
                        // when we onAssert this as a result for a query
                        // returning multiple results, it is already cached.
                        // If we wouldn't do this here, we'd start another
                        // query for this resource.
                        var messageHash = caller.mMessagesHash;
                        if (!messageHash.hasOwnProperty(newURL.spec)) {
                            messageHash[newURL.spec] = aMessage;
                        }

                        if (queryType == MESSAGE) {
                            // XXX - we need to onAssert this resource. However,
                            // we need to know somehow what the caller wanted
                            // to use this delegate for, so that we can limit
                            // the onAssert to what was asked for. This might be
                            // tricky, as we'd need to keep track of multiple
                            // "questions" coming in while the LDAP server
                            // hasn't responded yet.
                        }
                        else {
                            // Add the LDAP message to the result array for
                            // the query resource and onAssert the result
                            // as a child of the query resource.
                            var queryResource =
                                caller.mRdfSvc.GetResource(queryURL.spec);
                            if (!listHash.hasOwnProperty(queryURL.spec)) {
                                // No entry for the query resource in the
                                // results hashlist, let's create one.
                                listHash[queryURL.spec] = Components.classes
                                    ["@mozilla.org/supports-array;1"].
                                    createInstance(
                                       Components.interfaces.nsISupportsArray);
                            }
                            listHash[queryURL.spec].AppendElement(newResource);
                            if (queryType == FLAT_LIST) {
                                caller.mLDAPDataSource.onAssert(
                                    caller.mLDAPDataSource, aOuter,
                                    caller.kNC_child, newResource);
                            }
                            else if (queryType == RECURSIVE_LIST) {
                                caller.mLDAPDataSource.onAssert(
                                    caller.mLDAPDataSource, queryResource,
                                    caller.kNC_child, newResource);
                                caller.mLDAPDataSource.onAssert(
                                    caller.mLDAPDataSource, aOuter,
                                    caller.kNC_recursiveChild, newResource);
                            }
                        }
                    }
                    else if (aMessage.type == aMessage.RES_SEARCH_RESULT) {
                        // We got all the results for the query.
                        // If we were looking for a list, let's check if we
                        // have an entry for the query resource in the result's
                        // hashlist. If we don't, add an empty array to help
                        // performance, otherwise we'll keep requerying for this
                        // query resource.
                        if (queryType == FLAT_LIST || queryType == RECURSIVE_LIST) {
                            if (!listHash.hasOwnProperty(queryURL.spec)) {
                                // No entry for the query resource in the
                                // results hashlist, let's create an empty one.
                                listHash[queryURL.spec] = Components.classes
                                    ["@mozilla.org/supports-array;1"].
                                    createInstance(
                                       Components.interfaces.nsISupportsArray);
                            }
                        }
                        delete caller.mInProgressHash[queryURL.spec];
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
        var queryURL;
        var queryType;

        if (aKey == "message.ldap") {
            queryType = MESSAGE;
        }
        else if (aKey == "flat.messagelist.ldap") {
            queryType = FLAT_LIST;
        }
        else if (aKey == "recursive.messagelist.ldap") {
            queryType = RECURSIVE_LIST;
        }

        switch (queryType) {
            case MESSAGE:
                if (this.mMessagesHash.hasOwnProperty(aOuter.Value)) {
                    return (this.mMessagesHash[aOuter.Value].QueryInterface(aIID));
                }
                break;
            case FLAT_LIST:
                if (this.mMessagesListHash.hasOwnProperty(aOuter.Value)) {
                    return (this.mMessagesListHash[aOuter.Value].QueryInterface(aIID));
                }
                break;
            case RECURSIVE_LIST:
                queryURL = Components.classes["@mozilla.org/network/ldap-url;1"]
                      .createInstance(Components.interfaces.nsILDAPURL);
                queryURL.spec = aOuter.Value;
                if (queryURL.scope == SCOPE_BASE) {
                    // Retarget the URL, asking for recursive children on
                    // a base URL should descend, so we do a one-level search.
                    queryURL.scope = SCOPE_ONELEVEL;
                }
                if (this.mMessagesListHash.hasOwnProperty(queryURL.spec)) {
                    return (this.mMessagesListHash[queryURL.spec].QueryInterface(aIID));
                }
                break;
        }

        if (queryURL == null) {
            queryURL = Components.classes["@mozilla.org/network/ldap-url;1"]
                  .createInstance(Components.interfaces.nsILDAPURL);
            queryURL.spec = aOuter.Value;
        }

        // make sure that this if this URL is for a messagelist, it
        // represents something other than a base search
        //
        if ((queryType == FLAT_LIST) && (queryURL.scope == SCOPE_BASE)) {
            if (DEBUG) {
                dump("Early return, asking for a list of children for an " +
                     "URL with a BASE scope\n\n");
            }
            throw Components.results.NS_ERROR_FAILURE;
        }

        if (!this.mInProgressHash.hasOwnProperty(queryURL.spec)) {
            this.Init();

            // XXX - not sure what this should be
            var key = queryURL.host + ":" + queryURL.port;

            // Get the LDAP service and request a connection to the server from
            // the service
            var server;
            try {
                server = this.mLDAPSvc.getServer(key);
            }
            catch (e) {
            }
            if (!server) {
                // Create a server object and let the service know about it
                // XXX - not sure if I initialize enough here
                server = Components.classes
                        ["@mozilla.org/network/ldap-server;1"].
                        createInstance(Components.interfaces.nsILDAPServer);
                server.key = key;
                server.url = queryURL;
                this.mLDAPSvc.addServer(server);
            }

            // Mark this URL as being in progress, to avoid requerying for the
            // same URL
            this.mInProgressHash[queryURL.spec] = 1;

            // get a connection object
            //
            var connection = Components.classes
                    ["@mozilla.org/network/ldap-connection;1"].
                    createInstance(Components.interfaces.nsILDAPConnection);
            connection.init(queryURL.host, queryURL.port, null,
                            generateGetTargetsBoundCallback(), null,
                            Components.interfaces.nsILDAPConnection.VERSION3);

            // XXXdmose - in this case, we almost certainly shouldn't be
            // falling through to an error case, but instead returning
            // something reasonable, since this is actually a success
            // condition
            //
            debug("falling through!\n");
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
        debug("*** Registering LDAP datasource components" +
              " (all right -- a JavaScript module!)\n");

        compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);

        compMgr.registerFactoryLocation(
                NS_LDAPDATASOURCE_CID,
                'LDAP RDF DataSource',
                NS_LDAPDATASOURCE_CONTRACTID,
                fileSpec, location,
                type);

        compMgr.registerFactoryLocation(
                NS_LDAPMESSAGERDFDELEGATEFACTORY_CID,
                'LDAP Message RDF Delegate',
                NS_LDAPMESSAGERDFDELEGATEFACTORY_CONTRACTID,
                fileSpec, location,
                type);

        compMgr.registerFactoryLocation(
                NS_LDAPMESSAGERDFDELEGATEFACTORY_CID,
                'LDAP Flat MessageList RDF Delegate',
                NS_LDAPFLATMESSAGELISTRDFDELEGATEFACTORY_CONTRACTID,
                fileSpec, location,
                type);

        compMgr.registerFactoryLocation(
                NS_LDAPMESSAGERDFDELEGATEFACTORY_CID,
                'LDAP Recursive MessageList RDF Delegate',
                NS_LDAPRECURSIVEMESSAGELISTRDFDELEGATEFACTORY_CONTRACTID,
                fileSpec, location,
                type);

        compMgr.registerFactoryLocation(
                NS_LDAPURLRDFDELEGATEFACTORY_CID,
                'LDAP URL RDF Delegate',
                NS_LDAPURLRDFDELEGATEFACTORY_CONTRACTID,
                fileSpec, location,
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

    // because of the way JS components work (the JS garbage-collector
    // keeps track of all the memory refs and won't unload until appropriate)
    // this ends up being a dummy function; it can always return true.
    //
    canUnload: function(compMgr) { return true; }
};

function NSGetModule(compMgr, fileSpec) { return nsLDAPDataSourceModule; }
