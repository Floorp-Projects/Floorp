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
import java.io.*;
import netscape.ldap.client.*;
import netscape.ldap.util.*;
import java.util.zip.CRC32;

/**
 * <CODE>LDAPCache</CODE> is the class that represents an
 * in-memory cache that you can use to reduce the number of
 * search requests sent to the LDAP server.
 * <P>
 *
 * Each item in the cache represents a search request and
 * its results.  Each item is uniquely identified by the
 * search criteria, which includes:
 * <P>
 *
 * <UL>
 * <LI>the host name and port number of the LDAP server
 * <LI>the base DN of the search
 * <LI>the search filter
 * <LI>the scope of the search
 * <LI>the attributes to be returned in the search results
 * <LI>the DN used to authenticate the client when binding
 *     to the server
 * <LI>the LDAP v3 controls specified in the search request
 * </UL>
 * <P>
 *
 * After a search request is cached, the results of any
 * subsequent search requests using the same criteria are 8
 * read from the cache.  Note that if any part of the
 * criteria differs (for example, if a different DN is used
 * when binding to the server or if a different set of
 * attributes to be returned is specified), the search
 * request is sent to the server.
 * <P>
 *
 * When you create the cache, you specify the maximum amount
 * of time that an item can be kept in the cache.  When an
 * item's age exceeds that time limit, the item is removed
 * from the cache.
 * <P>
 *
 * The cache also has a maximum size that you specify when
 * creating the cache.  If adding a new item exceeds the
 * maximum size of the cache, the first entries in the cache
 * are removed to make enough space for the new item.
 * <P>
 *
 * Finally, when creating the cache, you can specify a list
 * of the base DNs in search requests that you want to cache.
 * For example, if you specify <CODE>o=Airius.com</CODE> as
 * a base DN to cache, your client caches search requests
 * where the base DN is <CODE>o=Airius.com</CODE>.
 * <P>
 *
 * To specify that you want to use a cache for a particular
 * LDAP session, call the <CODE>setCache</CODE> method of
 * the <CODE>LDAPConnection</CODE> object that you are
 * working with.
 * <P>
 *
 * All clones of an <CODE>LDAPConnection</CODE> object share
 * the same <CODE>LDAPCache</CODE> object.
 * <P>
 *
 * The <CODE>LDAPCache</CODE> class includes methods for
 * getting statistics (such as hit rates) from the cache and
 * for flushing entries from the cache.
 * <P>
 *
 * @see netscape.ldap.LDAPConnection#setCache(netscape.ldap.LDAPCache)
 * @see netscape.ldap.LDAPConnection#getCache
 */
public class LDAPCache implements TimerEventListener {
    private Hashtable m_cache;
    private long m_timeToLive;
    private long m_maxSize;
    private String[] m_dns;
    private Vector m_orderedStruct;
    private long m_remainingSize = 0;
    /**
     * Delimiter used internally when creating keys
     * for the cache.
     */
    public static final String DELIM = "#";
    private Timer m_timer = null;
    private static long TIMEOUT = 60000;
    private long m_totalOpers = 0;
    private static final boolean m_debug = false;
    private long m_hits = 0;
    private long m_flushes = 0;


    /**
     * Constructs a new <CODE>LDAPCache</CODE> object, using the
     * specified maximum size of the cache (in bytes) and the maximum
     * age of cached items (in seconds).  When items in the cache
     * exceed this age, they are removed from the cache.
     * <P>
     *
     * @param ttl The maximum amount of time that an item can be cached
     *  (in seconds)
     * @param size The maximum size of the cache (in bytes)
     */
    public LDAPCache(long ttl, long size)
    {
        init(ttl, size);
    }

    /**
     * Constructs a new <CODE>LDAPCache</CODE> object, using the
     * specified maximum size of the cache (in bytes), and the maximum
     * age of cached items (in seconds), and an array of  the base DNs
     * of searches that you want to cache.  (For example,
     * if the array of base DNs includes <CODE>o=Airius.com</CODE>,
     * the cache stores search results if the base DN in the search
     * request is <CODE>o=Airius.com</CODE>.)
     * <P>
     *
     * @param ttl The maximum amount of time that an item can be cached
     *  (in seconds)
     * @param size The maximum size of the cache (in bytes)
     * @param dns The list of base DNs of searches that you want to cache.
     */
    public LDAPCache(long ttl, long size, String[] dns)
    {
        init(ttl, size);

        m_dns = new String[dns.length];
        if ((dns != null) && (dns.length > 0))
            for (int i=0; i<dns.length; i++) {
                m_dns[i] = (new DN(dns[i])).toString();
        }
    }

    /**
     * Gets the maximum size of the cache (in bytes).
     * <P>
     *
     * @return The maximum size of the cache (in bytes)
     */
    public long getSize()
    {
        return m_maxSize;
    }

    /**
     * Gets the maximum age allowed for cached items (in
     * seconds).  (Items that exceed this age are
     * removed from the cache.)
     * <P>
     *
     * @return The maximum age of items in the cache (in
     *  seconds).
     */
    public long getTimeToLive()
    {
        return m_timeToLive/1000;
    }

    /**
     * Gets the array of base DNs of searches to be cached.
     * (Search requests with these base DNs are cached.)
     * <P>
     *
     * @return The array of base DNs.
     */
    public String[] getBaseDNs()
    {
        return m_dns;
    }

    /**
     * Flush the entries identified by DN and scope from the cache.
     * <P>
     *
     * @param dn The distinguished name (or base DN) of the entries
     *  to be removed from the cache. Use this parameter in conjunction
     *  with <CODE>scope</CODE> to identify the entries that you want
     *  removed from the cache.  If this parameter is <CODE>null</CODE>,
     *  the entire cache is flushed.
     * @param scope The scope identifying the entries that you want
     *  removed from the cache. The value of this parameter can be
     *  one of the following:
     *  <UL>
     *  <LI><CODE>LDAPv2.SCOPE_BASE</CODE> (to remove the entry identified
     *      by <CODE>dn</CODE>)
     *  <LI><CODE>LDAPv2.SCOPE_ONE</CODE> (to remove the entries that
     *      have <CODE>dn</CODE> as their parent entry)
     *  <LI><CODE>LDAPv2.SCOPE_SUB</CODE> (to remove the entries in the
     *      subtree under <CODE>dn</CODE> in the directory)
     *  </UL>
     * <P>
     * @return <CODE>true</CODE> if the entry is removed from the cache,
     *  or <CODE>false</CODE> if the entry is not removed.
     */
    public synchronized boolean flushEntries(String dn, int scope) {

        if (m_debug)
            System.out.println("DEBUG: User request for flushing entry: dn "+
            dn+" and scope "+scope);
        // if the dn is null, invalidate the whole cache
        if (dn == null)
        {
            // reclaim all the cache spaces
            m_remainingSize = m_maxSize;
            m_cache.clear();
            m_orderedStruct.removeAllElements();
            return true;
        }

        DN dn2 = new DN(dn);

        Enumeration e = m_cache.keys();

        while(e.hasMoreElements()) {
            Long key = (Long)e.nextElement();
            Vector val = (Vector)m_cache.get(key);

            int j=1;
            int size2=val.size();

            for (; j<size2; j++) {
                String d = ((LDAPEntry)val.elementAt(j)).getDN();
                DN dn1 = new DN(d);

                if (dn1.equals(dn2))
                    break;
                if (scope == LDAPConnection.SCOPE_ONE) {
                    DN parentDN1 = dn1.getParent();
                    if (parentDN1.equals(dn2)) {
                        break;
                    }
                }
                if ((scope == LDAPConnection.SCOPE_SUB) &&
                    (dn1.isDescendantOf(dn2))) {
                    break;
                }
            }

            if (j < size2) {
                for (int k=0; k<m_orderedStruct.size(); k++) {
                    Vector v = (Vector)m_orderedStruct.elementAt(k);
                    if (key.equals((Long)v.elementAt(0))) {
                        m_orderedStruct.removeElementAt(k);
                        break;
                    }
                }
                m_cache.remove(key);
                if (m_debug)
                    System.out.println("DEBUG: Successfully removed entry ->"+key);

                return true;
            }
        }

        if (m_debug)
            System.out.println("DEBUG: The number of keys in the cache is "
            +m_cache.size());

        return false;
    }

    /**
     * Gets invoked when the timer expires.
     * @param e The timer event containing the timer itself.
     */
    public void timerExpired(TimerEvent e)
    {
        flushEntries();

        Timer t = (Timer)e.getSource();
        t.start();
    }

    /**
     * Gets the amount of available space (in bytes) left in the cache.
     * <P>
     *
     * @return The available space (in bytes) in the cache.
     */
    public long getAvailableSize() {
        return m_remainingSize;
    }

    /**
     * Gets the total number of requests for retrieving items from
     * the cache.  This includes both items successfully found in
     * the cache and items not found in the cache.
     * <P>
     *
     * @return The total number of requests for retrieving items from
     * the cache.
     */
    public long getTotalOperations() {
        return m_totalOpers;
    }

    /**
     * Gets the total number of requests which failed to find and
     * retrieve an item from the cache.
     * <P>
     *
     * @return The number of requests that did not find and retrieve
     *  an item in the cache.
     */
    public long getNumMisses() {
        return (m_totalOpers - m_hits);
    }

    /**
     * Gets the total number of requests which successfully found and
     * retrieved an item from the cache.
     * @return The number of requests that successfully found and
     *  retrieved an item from the cache.
     */
    public long getNumHits() {
        return m_hits;
    }

    /**
     * Gets the total number of entries that are flushed when timer expires
     *  and <CODE>flushEntries</CODE> is called.
     * <P>
     *
     * @return The total number of entries that are flushed when timer
     *  expires.
     */
    public long getNumFlushes() {
        return m_flushes;
    }

    /**
     * Create a key for a cache entry by concatenating all input parameters
     * @return The key for a cache entry
     * @exception LDAPException Thrown when failed to create key.
     */
    Long createKey(String host, int port, String baseDN, String filter,
      int scope, String[] attrs, String bindDN, LDAPConstraints cons)
      throws LDAPException {

        DN dn = new DN(baseDN);
        baseDN = dn.toString();

        if (m_dns != null) {
            int i=0;
            for (; i<m_dns.length; i++) {
                if (baseDN.equals(m_dns[i]))
                    break;
            }

            if (i >= m_dns.length)
                throw new LDAPException(baseDN+" is not a cached base DN",
                LDAPException.OTHER);
        }

        String key = null;

        key = appendString(baseDN);
        key = key+appendString(scope);
        key = key+appendString(host);
        key = key+appendString(port);
        key = key+appendString(filter);
        key = key+appendString(attrs);
        key = key+appendString(bindDN);

        LDAPControl[] serverControls = null;
        LDAPControl[] clientControls = null;

        // get server and client controls
        if (cons != null)
        {
            serverControls = cons.getServerControls();
            clientControls = cons.getClientControls();
        }

        if ((serverControls != null) && (serverControls.length > 0))
        {
            String[] objID = new String[serverControls.length];

            for (int i=0; i<serverControls.length; i++) {
                long val = getCRC32(serverControls[i].getValue());
                objID[i] = new Long(val).toString();
            }
            key = key + appendString(objID);
        }
        else
            key = key+appendString(0);

        if ((clientControls != null) && (clientControls.length > 0))
        {
            String[] objID = new String[clientControls.length];

            for (int i=0; i<clientControls.length; i++) {
                long val = getCRC32(clientControls[i].getValue());
                objID[i] = new Long(val).toString();
            }
            key = key + appendString(objID);
        }
        else
            key = key+appendString(0);

        long val = getCRC32(key.getBytes());
        return new Long(val);
    }

    /**
     * Gets the cache entry based on the specified key.
     * @param key The key for the cache entry.
     * @return The cache entry
     */
    synchronized Object getEntry(Long key) {
        Object obj = null;

        obj = m_cache.get(key);
        m_totalOpers++;

        if (m_debug) {
            if (obj == null)
                System.out.println("DEBUG: Entry whose key -> "+key+
                    " not found in the cache.");
            else
                System.out.println("DEBUG: Entry whose key -> "+key+
                    " found in the cache.");
        }

        if (obj != null)
            m_hits++;


        return obj;
    }

    /**
     * Flush entries which stays longer or equal to the time-to-live.
     */
    synchronized void flushEntries()
    {
        Vector v = null;
        boolean delete = false;

        Date date = new Date();
        long currTime = date.getTime();

        m_flushes = 0;
        while(true) {
            if (m_orderedStruct.size() <= 0)
                break;

            v = (Vector)m_orderedStruct.firstElement();
            long diff = currTime-((Long)v.elementAt(1)).longValue();
            if (diff >= m_timeToLive) {
                Long key = (Long)v.elementAt(0);

                if (m_debug)
                    System.out.println("DEBUG: Timer flush entry whose key is "+key);

                Vector entry = (Vector)m_cache.remove(key);
                m_remainingSize = m_remainingSize + ((Long)entry.firstElement()).longValue();

                // always delete the first one
                m_orderedStruct.removeElementAt(0);

                m_flushes++;
            }
            else
            break;
        }

        if (m_debug)
            System.out.println("DEBUG: The number of keys in the cache is "
                +m_cache.size());
    }

    /**
     * Add the entry to the hashtable cache and to the vector respectively.
     * The vector is used to keep track of the order of the entries being added.
     * @param key The key for the cache entry.
     * @param value The cache entry being added to the cache for the specified
     *        key.
     * @exception LDAPException Get thrown when failed to add the entry.
     */
    synchronized void addEntry(Long key, Object value) throws LDAPException
    {
        // if entry exists, dont perform add operation
        if (m_cache.get(key) != null)
            return;

        Vector v = (Vector)value;

        // assume the size of the key is 4 bytes
        long size = ((Long)v.elementAt(0)).longValue()+4;

        if (size > m_maxSize) {
            throw new LDAPException("Failed to add an entry to the cache since the new entry exceeds the cache size", LDAPException.OTHER);
        }

        v.setElementAt(new Long(size), 0);

        // if the size of entry being added is bigger than the spare space in the
        // cache
        if (size > m_remainingSize) {
            while (true) {
                Vector element = (Vector)m_orderedStruct.firstElement();
                Long str = (Long)element.elementAt(0);
                Vector val = (Vector)m_cache.remove(str);
                if (m_debug)
                    System.out.println("DEBUG: The spare size of the cache is not big enough "+
                        "to hold the new entry, deleting the entry whose key -> "+str);

                // always remove the first one
                m_orderedStruct.removeElementAt(0);
                m_remainingSize = m_remainingSize +
                    ((Long)val.elementAt(0)).longValue();
                if (m_remainingSize >= size)
                    break;
            }
        }

        m_remainingSize = m_remainingSize - size;
        m_cache.put(key, v);
        Vector element = new Vector();
        element.addElement(key);
        Date date = new Date();
        element.addElement(new Long(date.getTime()));
        m_orderedStruct.addElement(element);

        if (m_debug)
        {
            System.out.println("DEBUG: Adding a new entry whose key -> "+key);
            System.out.println("DEBUG: The current number of keys in the cache "+
            m_cache.size());
        }
    }

    /**
     * Gets the number of entries being cached.
     * @return The number of entries being cached.
     */
    int size()
    {
        return m_cache.size();
    }

    /**
     * Clean up
     */
    void cleanup() {
        m_timer.stop();
    }

    /**
     * Initialize the instance variables.
     */
    private void init(long ttl, long size)
    {
        m_cache = new Hashtable();
        m_timeToLive = ttl*1000;
        m_maxSize = size;
        m_remainingSize = size;
        m_dns = null;
        m_orderedStruct = new Vector();
        m_timer = new Timer(TIMEOUT);
        m_timer.addTimerExpiredEventListener((TimerEventListener)this);
        m_timer.start();
    }

    /**
     * Concatenate the specified integer with the delimiter.
     * @param str The string which concatenate with the delimiter.
     * @return The concatenated string
     */
    private String appendString(String str) {
        if (str == null)
            return "null"+DELIM;
        else
            return str.trim()+DELIM;
    }

    /**
     * Concatenate the specified integer with the delimiter.
     * @param num The integer which concatenate with the delimiter.
     * @return The concatenated string
     */
    private String appendString(int num) {
        return num+DELIM;
    }

    /**
     * Concatenate the specified string array with the delimiter.
     * @param str A string array.
     * @return The concatenated string
     */
    private String appendString(String[] str) {

        if ((str == null) || (str.length < 1))
            return "0"+DELIM;
        else {
            String[] sorted = new String[str.length];
            System.arraycopy( str, 0, sorted, 0, str.length );
            sortStrings(sorted);

            String s = sorted.length+DELIM;
            for (int i=0; i<sorted.length; i++)
                s = s+sorted[i].trim()+DELIM;
            return s;
        }
    }

    /**
     * Sorts the array of strings using bubble sort.
     * @param str The array of string being sorted. The str parameter contains
     * the sorted result.
     */
    private void sortStrings(String[] str) {

        for (int i=0; i<str.length; i++)
            str[i] = str[i].trim();

        for (int i=0; i<str.length-1; i++)
            for (int j=i+1; j<str.length; j++)
            {
                if (str[i].compareTo(str[j]) > 0)
                {
                    String t = str[i];
                    str[i] = str[j];
                    str[j] = t;
                }
            }
    }

    /**
     * Create a 32 bits CRC from the given byte array.
     */
    private long getCRC32(byte[] barray) {
        CRC32 crcVal = new CRC32();
        crcVal.update(barray);
        return crcVal.getValue();
    }
}

/**
 * Represents a timer which will timeout for every certain interval. It
 * provides methods to start, stop, or restart timer. It also provides
 * methods to register/deregister the event listeners.
 */
class Timer {

    private long m_timeout;
    private Thread t = null;
    private TimerEventListener listener;
    protected TimerEventListener stopListener = null;

    /**
     * Constructor with the specified timout.
     * @param timeout The timeout value in milliseconds.
     */
    Timer(long timeout) {
        m_timeout = timeout;
    }

    /**
     * Start the timer.
     */
    void start() {
        TimerRunnable trun = new TimerRunnable(this);
        t = new Thread(trun);
        t.start();
    }

    /**
     * Stop the timer.
     */
    void stop() {
        t.interrupt();
    }

    /**
     * Get the timeout value.
     * @return the timeout value.
     */
    long getTimeout() {
        return m_timeout;
    }

    /**
     * Notify the listener when the timer expires.
     */
    void fireExpiredEvent() {
        TimerEvent event;

        if (stopListener != null)
        {
            event = new TimerEvent(this);
            stopListener.timerExpired(event);
        }
    }

    /**
     * Add the listener to the queue who wants to be notified when the timer
     * expires.
     */
    void addTimerExpiredEventListener(TimerEventListener listener) {
        stopListener = listener;
    }

    /**
     * Remove the listener from the queue who will not get notified when the
     * timer expires.
     */
    void removeTimerExpiredEventListener(TimerEventListener listener) {
        stopListener = null;
    }
}

/**
 * Represents the starting point for the timer thread to execute.
 */
class TimerRunnable implements Runnable {
    Timer m_timer;

    /**
     * Constructor with the specified timer object.
     * @param t The timer
     */
    TimerRunnable(Timer t) {
        m_timer = t;
    }

    /**
     * The runnable waits until the timeout period has elapsed. It then notify
     * the registered listener who listens for the timeout event.
     */
    public void run() {

        synchronized(this) {
            try {
                this.wait(m_timer.getTimeout());
            } catch (InterruptedException e) {
                // This happens if the timer is stopped
            }
        }

        m_timer.fireExpiredEvent();
    }
}

/**
 * Represents the timer event. When the timer expires, it will notify
 * all the registered listeners which receive the timer event. The
 * listener can retrieve the source of the event from the timer event.
 */
class TimerEvent extends EventObject {

    /**
     * Constructor with the specified source of the timer event.
     * @param source The source of the timer event.
     */
    TimerEvent(Object source) {
        super(source);
    }
}

/**
 * The timer client needs to implement this interface if it wants to
 * receive the timeout event.
 */
interface TimerEventListener extends EventListener {
    /**
     * Gets invoked when the timer expires.
     * @param timeout Timeout event contains the source of the event which is
     *        the timer.
     */
    void timerExpired(TimerEvent timeout);
}

