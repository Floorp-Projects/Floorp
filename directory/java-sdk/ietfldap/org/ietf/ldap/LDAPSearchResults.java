/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 */
package org.ietf.ldap;

import java.util.*;
import org.ietf.ldap.client.*;
import org.ietf.ldap.client.opers.*;
import java.io.*;

/**
 * The results of an LDAP search operation, represented as an enumeration.
 * Note that you can only iterate through this enumeration once: if you
 * need to use these results more than once, make sure to save the
 * results in a separate location.
 * <P>
 *
 * You can also use the results of a search in progress to abandon that search
 * operation.
 * <P>
 *
 * @version 1.0
 * @see org.ietf.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean)
 * @see org.ietf.ldap.LDAPConnection#abandon(org.ietf.ldap.LDAPSearchResults)
 */
public class LDAPSearchResults implements Serializable {

    static final long serialVersionUID = -501692208613904825L;
    private Vector entries = null;
    private LDAPSearchQueue resultSource;
    private boolean searchComplete = false;
    private LDAPConnection connectionToClose;
    private LDAPConnection currConn;
    private boolean persistentSearch = false;
    private LDAPSearchConstraints currCons;
    private String currBase;
    private int currScope;
    private String currFilter;
    private String[] currAttrs;
    private boolean currAttrsOnly;
    private Vector referralResults = new Vector();
    private Vector exceptions;
    private int msgID = -1;

    // only used for the persistent search
    private boolean firstResult = false;

    /**
     * Constructs an enumeration of search results.
     * Note that this does not actually generate the results;
     * you need to call <CODE>LDAPConnection.search</CODE> to
     * perform the search and get the results.
     * @see org.ietf.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean)
     */
    public LDAPSearchResults() {
        entries = new Vector();
        connectionToClose = null;
        searchComplete = true;
        currCons = new LDAPSearchConstraints();
    }

    LDAPSearchResults( LDAPConnection conn,
                       LDAPSearchConstraints cons,
                       String base,
                       int scope,
                       String filter,
                       String[] attrs,
                       boolean attrsOnly ) {
        this();
        currConn = conn;
        currCons = cons;
        currBase = base;
        currScope = scope;
        currFilter = filter;
        currAttrs = attrs;
        currAttrsOnly = attrsOnly;
    }

    /**
     * Constructs an enumeration of search results. Used when returning results
     * from a cache.
     * @param v the vector containing LDAPEntries
     * @see org.ietf.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean)
     */
    LDAPSearchResults( Vector v ) {
        this();
        entries = (Vector)v.clone();

        if ((entries != null) && (entries.size() >= 1)) {
            // Each cache value is represented by a vector. The first element
            // represents the size of all the LDAPEntries. This needs to be
            // removed before we iterate through each LDAPEntry.
            entries.removeElementAt(0);
        }
    }

    LDAPSearchResults( Vector v,
                       LDAPConnection conn,
                       LDAPSearchConstraints cons,
                       String base,
                       int scope,
                       String filter,
                       String[] attrs,
                       boolean attrsOnly ) {
        this( v );
        currConn = conn;
        currCons = cons;
        currBase = base;
        currScope = scope;
        currFilter = filter;
        currAttrs = attrs;
        currAttrsOnly = attrsOnly;
    }

    /**
     * Returns a count of queued search results immediately available for
     * processing.
     * A search result is either a search entry or an exception. If the
     * search is asynchronous (batch size not 0), this reports the number
     * of results received so far.
     * @return count of search results immediatly available for processing
     */
    public int getCount() {

        while (resultSource != null && resultSource.getMessageCount() > 0) {
            fetchResult();
        }

        int count = entries.size();
        
        for ( int i = 0; i < referralResults.size(); i++ ) {
            LDAPSearchResults res =
                (LDAPSearchResults)referralResults.elementAt(i);
            count += res.getCount();
        } 

        if ( exceptions != null ) {
            count += exceptions.size();
        } 

        return count;
    }

    /**
     * Returns the controls returned with this search result. If any control
     * is registered with <CODE>LDAPControl</CODE>, an attempt is made to
     * instantiate the control. If the instantiation fails, the control is
     * returned as a basic <CODE>LDAPControl</CODE>.
     * @return an array of type <CODE>LDAPControl</CODE>.
     * @see org.ietf.ldap.LDAPControl#register
     */
    public LDAPControl[] getResponseControls() {
        return currConn.getResponseControls(msgID);
    }

    /**
     * Returns <CODE>true</CODE> if there are more search results
     * to be returned. You can use this method in conjunction with the
     * <CODE>next</CODE> or <CODE>next</CODE> methods to iterate
     * through each entry in the results. For example:
     * <PRE>
     * LDAPSearchResults res = ld.search( MY_SEARCHBASE,
     *                         LDAPConnection.SCOPE_BASE, MY_FILTER,
     *                         null, false );
     * while ( res.hasMore() ) {
     *   LDAPEntry findEntry = res.next();
     *   ...
     * }
     * </PRE>
     * @return <CODE>true</CODE> if there are more search results.
     * @see org.ietf.ldap.LDAPSearchResults#next()
     */
    public boolean hasMore() {

        while ((entries.size() == 0) && (!searchComplete)) {
            fetchResult();
        }

        if ((entries.size() == 0) && 
          ((exceptions == null) || (exceptions.size() == 0))) {
            while (referralResults.size() > 0) {
                LDAPSearchResults res =
                  (LDAPSearchResults)referralResults.elementAt(0);
                if (res.hasMore())
                    return true;
                else
                    referralResults.removeElementAt(0);
            }
        }

        return ((entries.size() > 0) || 
          ((exceptions != null) && (exceptions.size() > 0)));
    }

    /**
     * Returns the next LDAP entry from the search results
     * and throws an exception if the next result is a referral, or 
     * if a sizelimit or timelimit error occurred.
     * <P>
     *
     * You can use this method in conjunction with the
     * <CODE>hasMore</CODE> method to iterate through
     * each entry in the search results.  For example:
     * <PRE>
     * LDAPSearchResults res = ld.search( MY_SEARCHBASE,
     *                         LDAPConnection.SCOPE_BASE, MY_FILTER,
     *                         null, false );
     * while ( res.has() ) {
     *   try {
     *     LDAPEntry findEntry = res.next();
     *   } catch ( LDAPReferralException e ) {
     *     String refUrls[] = e.getReferrals();
     *     for ( int i = 0; i < refUrls.length; i++ ) {
     *     // Your code for handling referrals
     *     }
     *     continue;
     *   } catch ( LDAPException e ) {
     *     // Your code for handling errors on limits exceeded 
     *     continue; 
     *   } 
     *   ...
     * }
     * </PRE>
     * @return the next LDAP entry in the search results.
     * @exception LDAPReferralException A referral (thrown
     * if the next result is a referral), or LDAPException 
     * if a limit on the number of entries or the time was 
     * exceeded.
     * @see org.ietf.ldap.LDAPSearchResults#hasMore()
     */
    public LDAPEntry next() throws LDAPException {
        Object o = nextElement();
        if ((o instanceof LDAPReferralException) ||
            (o instanceof LDAPException)) {
            throw (LDAPException)o;
        }
        if (o instanceof LDAPEntry) {
            return (LDAPEntry)o;
        }
        return null;
    }

    /**
     * Sorts the search results.
     * <P>
     *
     * The comparator determines the sort order used.  For example, if the
     * comparator uses the <CODE>uid</CODE>
     * attribute for comparison, the search results are sorted according to
     * <CODE>uid</CODE>.
     * <P>
     *
     * The following section of code sorts results in ascending order,
     * first by surname and then by common name.
     *
     * <PRE>
     * String[]  sortAttrs = {"sn", "cn"};
     * boolean[] ascending = {true, true};
     *
     * LDAPConnection ld = new LDAPConnection();
     * ld.connect( ... );
     * LDAPSearchResults res = ld.search( ... );
     * res.sort( new LDAPCompareAttrNames(sortAttrs, ascending) );
     * </PRE>
     * NOTE: If the search results arrive asynchronously, the <CODE>sort</CODE>
     * method blocks until all the results are returned.
     * <P>
     *
     * If some of the elements of the Enumeration have already been fetched,
     * the cursor is reset to the (new) first element.
     * <P>
     *
     * @param compare comparator used to determine the sort order of the results
     * @see LDAPCompareAttrNames
     */
    public synchronized void sort(Comparator compare) {

        // if automatic referral, then add to the entries, otherwise, dont do it
        // since the elements in referralResults are LDAPReferralException.
        if (currCons.getReferralFollowing()) {
            while (referralResults.size() > 0) {
                Object obj = null;
                if ((obj=nextReferralElement()) != null) {
                    if (obj instanceof LDAPException) {
                        add((LDAPException)obj); // put it back
                    }                    
                    else {
                        entries.addElement(obj);                      
                    }
                }
            }
        }

        int numEntries = entries.size();
        if (numEntries <= 0) {
            return;
        }

        LDAPEntry[] toSort = new LDAPEntry[numEntries];
        entries.copyInto (toSort);

        if (toSort.length > 1) {
            quicksort (toSort, compare, 0, numEntries-1);
        }

        entries.removeAllElements();
        for (int i = 0; i < numEntries; i++) {
            entries.addElement (toSort[i]);
        }
    }

    /**
     * Adds a search entry or referral
     *
     * @param msg LDAPSearchResult or LDAPsearchResultReference
     */
    void add( LDAPMessage msg ) {
        if ( msg instanceof LDAPSearchResult ) {
            entries.addElement( ((LDAPSearchResult)msg).getEntry());
        } else if ( msg instanceof LDAPSearchResultReference ) {
            /* convert to LDAPReferralException */
            String urls[] = ((LDAPSearchResultReference)msg).getReferrals();
            if ( urls != null ) {
                if (exceptions == null) {
                    exceptions = new Vector();
                }                    
                exceptions.addElement(
                    new LDAPReferralException(null, 0, urls) );
            }
        }            
    }

    /**
     * Adds and exception
     * @param e exception
     */
    void add( LDAPException e ) {
        if ( exceptions == null ) {
            exceptions = new Vector();
        }
        exceptions.addElement( e );
    }

    /**
     * Prepares to return asynchronous results from a search
     *
     * @param l Listener which will provide results
     */
    void associate( LDAPSearchQueue l ) {
        resultSource = l;
        searchComplete = false;
    }

    void associatePersistentSearch( LDAPSearchQueue l) {
        resultSource = l;
        persistentSearch = true;
        searchComplete = false;
        firstResult = true;
    }

    void addReferralEntries( LDAPSearchResults res ) {
        referralResults.addElement( res );
    }

    /**
     * For asynchronous search, this mechanism allows the programmer to
     * close a connection whenever the search completes.
     * @param toClose connection to close when the search terminates
     */
    void closeOnCompletion( LDAPConnection toClose ) {
        if ( searchComplete ) {
            try {
                toClose.disconnect();
            } catch( LDAPException e ) {
            }
        } else {
            connectionToClose = toClose;
        }
    }


    /**
     * Basic quicksort algorithm.
     */
    void quicksort ( LDAPEntry[] toSort, Comparator compare,
                     int low, int high ) {
        if (low >= high) {
            return;
        }

        LDAPEntry pivot = toSort[low];
        int slow = low-1, shigh = high+1;

        while (true) {
            do {
                shigh--;
            } while ( compare.compare(toSort[shigh], pivot) > 0 );
            do {
                slow++;
            } while ( compare.compare(pivot, toSort[slow]) > 0 );

            if (slow >= shigh) {
                 break;
            }

             LDAPEntry temp = toSort[slow];
             toSort[slow] = toSort[shigh];
             toSort[shigh] = temp;
        }

        quicksort (toSort, compare, low, shigh);
        quicksort (toSort, compare, shigh+1, high);
    }

    /**
     * Sets the message ID for this search request. msgID is used
     * to retrieve response controls.
     * @param msgID Message ID for this search request
     */
    void setMsgID( int msgID ) {
        this.msgID = msgID;
    }
    
    /**
     * Returns the next result from a search.  You can use this method
     * in conjunction with the <CODE>hasMore</CODE> method to
     * iterate through all elements in the search results.
     * <P>
     *
     * Make sure to cast the
     * returned element as the correct type.  For example:
     * <PRE>
     * LDAPSearchResults res = ld.search( MY_SEARCHBASE,
     *                         LDAPConnection.SCOPE_BASE, MY_FILTER,
     *                         null, false );
     * while ( res.hasMore() ) {
     *   Object o = res.next(); 
     *   if ( o instanceof LDAPEntry ) { 
     *     LDAPEntry findEntry = (LDAPEntry)o; 
     *     ... 
     *   } else if ( o instanceof LDAPReferralException ) { 
     *     LDAPReferralException e = (LDAPReferralException)o; 
     *     String refUrls[] = e.getReferrals(); 
     *     ... 
     *   } else if ( o instanceof LDAPException ) { 
     *     LDAPException e = (LDAPException)o; 
     *     ... 
     *   } 
     * } 
     * </PRE> 
     * @return the next element in the search results.
     * @see org.ietf.ldap.LDAPSearchResults#hasMore()
     */
    Object nextElement() {
        if ( entries.size() > 0 ) {
            Object obj = entries.elementAt(0);
            entries.removeElementAt(0);
            return obj;
        }

        if (referralResults.size() > 0) {
            return nextReferralElement();
        }

        if ((exceptions != null) && (exceptions.size() > 0)) {
            Object obj = exceptions.elementAt(0);
            exceptions.removeElementAt(0);
            return obj;
        }

        return null;
    }

    Object nextReferralElement() {
        LDAPSearchResults res =
          (LDAPSearchResults)referralResults.elementAt(0);
        if ((!res.persistentSearch && res.hasMore()) || 
          (res.persistentSearch)) {
            Object obj = res.nextElement();
            if (obj != null) {
                return obj;
            }

            if ((obj == null) || (!res.hasMore())) {
                referralResults.removeElementAt(0);
            }
        } else {
            referralResults.removeElementAt(0);
        }

        return null;
    }

    /**
     * Returns message ID.
     * @return Message ID.
     */
    int getMessageID() {
        if ( resultSource == null ) {
            return -1;
        }
        return resultSource.getMessageID();
    }

    /**
     * Fetchs the next result, for asynchronous searches.
     */
    private synchronized void fetchResult() {

        /* Asynchronous case */
        if ( resultSource != null ) {
            synchronized( this ) {
                if (searchComplete || firstResult) {
                    firstResult = false;
                    return;
                }

                LDAPMessage msg = null;
                try {
                    msg = resultSource.getResponse();
                } catch (LDAPException e) {
                    add(e);
                    currConn.releaseSearchListener(resultSource);
                    searchComplete = true;
                    return;
                }
                    

                if (msg == null) { // Request abandoned
                    searchComplete = true;
                    currConn.releaseSearchListener(resultSource); 
                    return; 

                } else if (msg instanceof LDAPResponse) {
                    try {
                        // check response and see if we need to do referral
                        // v2: referral stored in the JDAPResult
                        currConn.checkSearchMsg(this, msg, currCons,
                                                currBase, currScope, currFilter,
                                                currAttrs, currAttrsOnly);
                  } catch (LDAPException e) {
                      System.err.println("LDAPSearchResults.fetchResult: "+e);
                  } finally {
                      currConn.releaseSearchListener(resultSource);
                  }
                  searchComplete = true;
                  if (connectionToClose != null) {
                      try {
                          connectionToClose.disconnect ();
                      } catch (LDAPException e) {
                      }
                      connectionToClose = null;
                  }
                  return;
                } else {
                    try {
                        currConn.checkSearchMsg(this, msg, currCons,
                          currBase, currScope, currFilter, currAttrs, currAttrsOnly);
                    } catch (LDAPException e) {
                        System.err.println("Exception: "+e);
                    }
                }
            }
        }
    }
}
