/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
package netscape.ldap;

import java.util.*;
import netscape.ldap.client.*;
import netscape.ldap.client.opers.*;
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
 * @see netscape.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean)
 * @see netscape.ldap.LDAPConnection#abandon(netscape.ldap.LDAPSearchResults)
 */
public class LDAPSearchResults implements Enumeration {

    private Vector entries = null;
    private LDAPSearchListener resultSource;
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

    // only used for the persistent search
    private boolean firstResult = false;

    /**
     * Constructs an enumeration of search results.
     * Note that this does not actually generate the results;
     * you need to call <CODE>LDAPConnection.search</CODE> to
     * perform the search and get the results.
     * @see netscape.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean)
     */
    public LDAPSearchResults() {
        entries = new Vector();
        connectionToClose = null;
        searchComplete = true;
    }

    LDAPSearchResults(LDAPConnection conn, LDAPSearchConstraints cons,
      String base, int scope, String filter, String[] attrs, boolean attrsOnly) {
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
     * @param v The vector containing LDAPEntries.
     * @see netscape.ldap.LDAPConnection#search(java.lang.String, int, java.lang.String, java.lang.String[], boolean)
     */
    LDAPSearchResults(Vector v) {
        this();
        entries = (Vector)v.clone();

        if ((entries != null) && (entries.size() >= 1)) {
            // Each cache value is represented by a vector. The first element
            // represents the size of all the LDAPEntries. This needs to be
            // removed before we iterate through each LDAPEntry.
            entries.removeElementAt(0);
        }
    }

    LDAPSearchResults(Vector v, LDAPConnection conn, LDAPSearchConstraints cons,
      String base, int scope, String filter, String[] attrs, boolean attrsOnly) {
        this(v);
        currConn = conn;
        currCons = cons;
        currBase = base;
        currScope = scope;
        currFilter = filter;
        currAttrs = attrs;
        currAttrsOnly = attrsOnly;
    }

    void add( JDAPProtocolOp sr ) {
        if (sr instanceof JDAPSearchResponse)
            add((JDAPSearchResponse)sr);
        else if (sr instanceof JDAPSearchResultReference)
            add((JDAPSearchResultReference)sr);
    }

    /**
     * Adds one result of a search (from JDAP) to the object.
     * @param sr A search response in JDAP format
     */
    void add( JDAPSearchResponse sr ) {
        LDAPAttribute[] lattrs = sr.getAttributes();
        LDAPAttributeSet attrs;
        if ( lattrs != null )
            attrs = new LDAPAttributeSet( lattrs );
        else
            attrs = new LDAPAttributeSet();

        String dn = sr.getObjectName();
        LDAPEntry entry = new LDAPEntry( dn, attrs );
        entries.addElement( entry );
    }

    /**
     * Adds search reference to this object.
     */
    void add( JDAPSearchResultReference sr ) {
        /* convert to LDAPReferralException */
        String urls[] = sr.getUrls();
        if (urls != null) {
            if (exceptions == null)
                exceptions = new Vector();
            exceptions.addElement(new LDAPReferralException(null, 0, urls));
        }
    }

    void add(LDAPException e) {
        if (exceptions == null)
            exceptions = new Vector();
        exceptions.addElement(e);
    }

    /**
     * Prepares to return asynchronous results from a search
     * @param l Listener which will provide results
     */
    void associate( LDAPSearchListener l) {
        resultSource = l;
        searchComplete = false;
    }

    void associatePersistentSearch( LDAPSearchListener l) {
        resultSource = l;
        persistentSearch = true;
        searchComplete = false;
        firstResult = true;
    }

    void addReferralEntries(LDAPSearchResults res) {
        referralResults.addElement(res);
    }

    /**
     * For asynchronous search, this mechanism allows the programmer to
     * close a connection whenever the search completes.
     * @param toClose Connection to close when the search terminates
     */
    void closeOnCompletion (LDAPConnection toClose) {
        if (searchComplete) {
            try {
                toClose.disconnect();
            } catch (LDAPException e) {
            }
        } else {
            connectionToClose = toClose;
        }
    }


    /**
     * Basic quicksort algorithm.
     */
    void quicksort (LDAPEntry[] toSort, LDAPEntryComparator compare,
                     int low, int high) {
        if (low >= high)
            return;

        LDAPEntry pivot = toSort[low];
        int slow = low-1, shigh = high+1;

        while (true) {
            do
                shigh--;
            while (compare.isGreater (toSort[shigh], pivot));
            do
                slow++;
            while (compare.isGreater (pivot, toSort[slow]));

            if (slow >= shigh)
                 break;

             LDAPEntry temp = toSort[slow];
             toSort[slow] = toSort[shigh];
             toSort[shigh] = temp;
        }

        quicksort (toSort, compare, low, shigh);
        quicksort (toSort, compare, shigh+1, high);
    }

    /**
     * Sorts the search results.
     * <P>
     *
     * The comparator (<CODE>LDAPEntryComparator</CODE>) determines the
     * sort order used.  For example, if the comparator uses the <CODE>uid</CODE>
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
     * @param compare Comparator used to determine the sort order of the results.
     * @see LDAPEntryComparator
     */
    public synchronized void sort(LDAPEntryComparator compare) {
        while (!searchComplete) {
            fetchResult();
        }

        // if automatic referral, then add to the entries, otherwise, dont do it
        // since the elements in referralResults are LDAPReferralException.
        if (currCons.getReferrals())
            while (referralResults.size() > 0) {
                Object obj = null;
                if ((obj=nextReferralElement()) != null)
                    entries.addElement(obj);
        }

        int numEntries = entries.size();
        if (numEntries <= 0)
            return;

        LDAPEntry[] toSort = new LDAPEntry[numEntries];
        entries.copyInto (toSort);

        if (toSort.length > 1)
            quicksort (toSort, compare, 0, numEntries-1);

        entries.removeAllElements();
        for (int i = 0; i < numEntries; i++)
            entries.addElement (toSort[i]);
    }

    /**
     * Returns the next LDAP entry from the search results
     * and throws an exception if the next result is a referral, or 
     * if a sizelimit or timelimit error occurred.
     * <P>
     *
     * You can use this method in conjunction with the
     * <CODE>hasMoreElements</CODE> method to iterate through
     * each entry in the search results.  For example:
     * <PRE>
     * LDAPSearchResults res = ld.search( MY_SEARCHBASE,
     *                         LDAPConnection.SCOPE_BASE, MY_FILTER,
     *                         null, false );
     * while ( res.hasMoreElements() ) {
     *   try {
     *     LDAPEntry findEntry = res.next();
     *   } catch ( LDAPReferralException e ) {
     *     LDAPUrl refUrls[] = e.getURLs();
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
     * @return The next LDAP entry in the search results.
     * @exception LDAPReferralException A referral (thrown
     * if the next result is a referral), or LDAPException 
     * if a limit on the number of entries or the time was 
     * exceeded.
     * @see netscape.ldap.LDAPSearchResults#hasMoreElements()
     */
    public LDAPEntry next() throws LDAPException {
        Object o = nextElement();
        if ((o instanceof LDAPReferralException) ||
            (o instanceof LDAPException))
            throw (LDAPException)o;
        if (o instanceof LDAPEntry)
            return (LDAPEntry)o;
        return null;
    }

    /**
     * Returns the next result from a search.  You can use this method
     * in conjunction with the <CODE>hasMoreElements</CODE> method to
     * iterate through all elements in the search results.
     * <P>
     *
     * Make sure to cast the
     * returned element as the correct type.  For example:
     * <PRE>
     * LDAPSearchResults res = ld.search( MY_SEARCHBASE,
     *                         LDAPConnection.SCOPE_BASE, MY_FILTER,
     *                         null, false );
     * while ( res.hasMoreElements() ) {
     *   Object o = res.nextElement(); 
     *   if ( o instanceof LDAPEntry ) { 
     *     LDAPEntry findEntry = (LDAPEntry)o; 
     *     ... 
     *   } else if ( o instanceof LDAPReferralException ) { 
     *     LDAPReferralException e = (LDAPReferralException)o; 
     *     LDAPUrl refUrls[] = e.getURLs(); 
     *     ... 
     *   } else if ( o instanceof LDAPException ) { 
     *     LDAPException e = (LDAPException)o; 
     *     ... 
     *   } 
     * } 
     * </PRE> 
     * @return The next element in the search results.
     * @see netscape.ldap.LDAPSearchResults#hasMoreElements()
     */
    public Object nextElement() {
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
        if ((!res.persistentSearch && res.hasMoreElements()) || 
          (res.persistentSearch)) {
            Object obj = res.nextElement();
            if (obj != null) {
                return obj;
            }

            if ((obj == null) || (!res.hasMoreElements()))
                referralResults.removeElementAt(0);
        }
        else
            referralResults.removeElementAt(0);

        return null;
    }

    /**
     * Returns <CODE>true</CODE> if there are more search results
     * to be returned. You can use this method in conjunction with the
     * <CODE>nextElement</CODE> or <CODE>next</CODE> methods to iterate
     * through each entry in the results. For example:
     * <PRE>
     * LDAPSearchResults res = ld.search( MY_SEARCHBASE,
     *                         LDAPConnection.SCOPE_BASE, MY_FILTER,
     *                         null, false );
     * while ( res.hasMoreElements() ) {
     *   LDAPEntry findEntry = (LDAPEntry)res.nextElement();
     *   ...
     * }
     * </PRE>
     * @return <CODE>true</CODE> if there are more search results
     * @see netscape.ldap.LDAPSearchResults#nextElement()
     * @see netscape.ldap.LDAPSearchResults#next()
     */
    public boolean hasMoreElements() {

        while ((entries.size() == 0) && (!searchComplete))
            fetchResult();

        if ((entries.size() == 0) && 
          ((exceptions == null) || (exceptions.size() == 0))) {
            while (referralResults.size() > 0) {
                LDAPSearchResults res =
                  (LDAPSearchResults)referralResults.elementAt(0);
                if (res.hasMoreElements())
                    return true;
                else
                    referralResults.removeElementAt(0);
            }
        }

        return ((entries.size() > 0) || 
          ((exceptions != null) && (exceptions.size() > 0)));
    }

    /**
     * Returns a count of the entries in the search results.
     * @return Count of entries found by the search.
     */
    public int getCount() {
        int totalReferralEntries = 0;
        int count = 0;
        for (int i=0; i<referralResults.size(); i++) {
            LDAPSearchResults res =
              (LDAPSearchResults)referralResults.elementAt(i);
            totalReferralEntries = totalReferralEntries+res.getCount();
        }
        if (resultSource != null) {
            count = resultSource.getCount();
        }     
        if (exceptions != null)
            return (count + exceptions.size() + totalReferralEntries);
        return (count + totalReferralEntries);
    }

    /**
     * Returns message id.
     * @return Message id.
     */
    int getID() {
        if ( resultSource == null )
            return -1;
        return resultSource.getID();
    }

    /**
     * Terminates the iteration; called on LDAPConnection.abandon().
     */
    void abandon() {
        synchronized( this ) {
            searchComplete = true;
        }
    }

    /**
     * Fetchs the next result, for asynchronous searches.
     */
    private synchronized void fetchResult() {

        /* Asynchronous case */
        if ( resultSource != null ) {
            synchronized( this ) {
                if (searchComplete || firstResult)
                {
                    firstResult = false;
                    return;
                }

                JDAPMessage msg = resultSource.nextResult();
                if (msg == null) {
                  try {
                      // check response and see if we need to do referral
                      // v2: referral stored in the JDAPResult
                      JDAPMessage response = resultSource.getResponse();
                      currConn.checkSearchMsg(this, response, currCons,
                        currBase, currScope, currFilter, currAttrs, currAttrsOnly);
                  } catch (LDAPException e) {
                      System.err.println("Exception: "+e);
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
