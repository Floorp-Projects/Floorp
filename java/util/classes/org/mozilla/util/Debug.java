/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The original code is "The Lighthouse Foundation Classes (LFC)"
 * 
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are Copyright (C) 1997, 1998, 1999 Sun
 * Microsystems, Inc. All Rights Reserved.
 */


package org.mozilla.util;

import java.util.Vector;
import java.util.Enumeration;
import java.util.Date;

/**
 * <P>
 * <B>Debug</B> Vendor of debug "filter" strings set & queried by clients. This allows
 * conditional code to only be executed if a certain filter is set.<BR>
 *
 * <I>Example of use from <B>JAG</B> days:<BR>
 * JDApplication allows the setting of filters from the commandline at app startup time,
 * and Log supports printing debug messages only when a specific filter string has
 * been set.</I>
 * 
 * <P>
 *
 * Alternatively, users can use the System Properties table to define
 * filter strings at runtime: <P>
 *
 * <CODE>
 * java -DDebug.filters=String ... <P>
 * </CODE>
 *
 * where String is comma (,) separated list of constants <B>WITH NO
 * WHITESPACE</B>.  ie "AXISPANEL_PAINT,BODYPANEL_PAINT". <P>
 *
 *
 * All filters are case-sensitive.
 *
 * This class also provides various timing routines.
 *
 * </P>
 * @author Keith Bernstein
 * @version $Id: Debug.java,v 1.1 1999/07/30 01:02:57 edburns%acm.org Exp $ */

public class Debug extends Object {
    static public final String HELP_FILTER_STRING = "HELP";
    static public final String ALL_FILTER_STRING = "ALL";
    static public final String TIMING_FILTER_STRING = "TIMING";
    static public final String PROGRESS_FILTER_STRING = "PROGRESS";

    /** the name in the properties table where we look for filter strings */
    static final String PROPERTY_NAME = "Debug.filters";
    static final String SEP = ",";

    static boolean showedColumnTitleHelp = false;
    // PENDING(kbern) When we start using JDK 1.2, we should change this filters Vector to a
    // "Set" in the new collection API for increased access and verification times.
    static Vector filters = null;
    static long initializationTime = new Date().getTime();
    static long lastTime = initializationTime;
    static long startTime = 0L;
    static long lapTime = 0L;

    static {
	/** This code adds to the filters Vector from the
	 * System.Properties variable named by PROPERTY_NAME
	 * The value of this variable must be a SEP separeted list
	 * and contain *NO WHITESPACE*!!
	 */
	String flags = System.getProperty(PROPERTY_NAME);
	
	if (null != flags) {
	    Vector flagVector = Utilities.vectorFromString(flags, SEP);
	    int i, size = (null != flagVector) ? flagVector.size() : 0;
	    String curFlag;
	    
	    for (i = 0; i < size; i++) {
		curFlag = (String) flagVector.elementAt(i);
		Debug.addFilter(curFlag);
	    }
	}
    }

    /**
    * Sets a debug filter, for future consumption by this class, as well as other
    * utility classes, like Log, etc.
    *
    * Virtually all "filters" are simply developer-meaningful strings which will be
    * tested within developer code, to conditionally execute code.
    *
    * There are some predefined filters, which this class actually does something with
    * (besides simply handing it back when asked for).
    * 
    * The predefined filters are:
    *	HELP
    *	ALL
    *	TIMING
    *
    * If the "HELP" filter is found, this class will print a message displaying the
    * predefined filters and what they do.
    *
    * If the "ALL" filter is found, this class will return "true" when queried for the 
    * existence of <B>any</B> filter, effectively turning on all debugging tests.
    * This is useful for both quick and easy tests, as well as to find forgotten debug
    * filters (see Log class for more info on this).
    *
    * If the "TIMING" filter is specified, then the time routines will always print
    * their info, regardless of what filter string is passed to them. This is useful
    * for turning on all timing tests.
    * */
    static public synchronized void addFilter(String aFilter) {
	if (aFilter != null) {
	    if (filters == null) {
		Log.log("Debugging has been enabled.");
		Log.log("os name: "+System.getProperty("os.name"));
		Log.log("os version: "+System.getProperty("os.version"));
		Log.log("Java version: "+System.getProperty("java.version"));
		Log.log("Java home: "+System.getProperty("java.home"));
		Log.log("User home: "+System.getProperty("user.home"));
		filters = new Vector(1);
	    }
	    Log.log("Adding debug filter: "+aFilter);
	    if (!filters.contains(aFilter)) {
		filters.addElement(aFilter);
	    }
	    if (aFilter.equalsIgnoreCase(Debug.HELP_FILTER_STRING)) {
		Log.log("Set one or more debug filters (usually possible from the commandling), then simply wrap your debug code in a test for a particular debug filter, and only execute the code if that filter has been set.");
		Log.log("Predifined debug filters:");
		Log.log("    HELP   [prints this info]");
		Log.log("    ALL    [causes all calls to \"containsFilter()\" to return \"true\", effectively enabling all filters]");
		Log.log("    TIMING [causes all timing messages generated by this class to print]");
	    }
	} else {
	    throw new IllegalArgumentException("null filter passed to addFilter()");
	}
    }
    
    /**
    * Removes the specified filter from the list of filters.
    */
    static public synchronized void removeFilter(String aFilter) {
        if (filters != null) {
            filters.removeElement(aFilter);
        }
    }
    
    /**
    * Removes all filters from the list of filters.
    */
    static public synchronized void removeAllFilters() {
        if (filters != null) {
            filters.removeAllElements();
        }
    }
    
    /**
    * Look for any "filter" with the specified prefix.
    *
    * "ALL" is not considered to be a match.
    *
    * This method works in (normally) O-n time (if no match).
    */
    
    static public synchronized boolean containsFilterWithPrefix(String aFilterPrefix) {
	boolean returnValue = false;
	
	if (Debug.filters != null) {
	    Enumeration filterEnumeration = Debug.filters.elements();
	    
	    while (!returnValue && filterEnumeration.hasMoreElements()) {
		String aFilter = (String)filterEnumeration.nextElement();
		
		if (aFilter.startsWith(aFilterPrefix)) {
		    returnValue = true;
		}
	    }
	}
	return returnValue;
    }
    
    /**
    * Funnel-point method, which takes a filter and an "allFiltersgMatchThisString" string.
    * See the javadoc for "containsFilter(String aFilter)" for the rest of what this method
    * does.
    *
    * This method works in (normally) O-1 time.
    *
    * NOTE: The "allFiltersgMatchThisString" parameter can be used to conditionally execute
    * code while preventing the "ALL" filter from having any effect. So, the conditional code
    * should use the test "if (Debug.containsFilter("SomeFilter", ""))" to see if a filter
    * has been set, and not get a false positive from the "ALL" filter.
    */
    static public synchronized boolean containsFilter(String aFilter, String allFiltersgMatchThisString) {
	if (((aFilter != null) && ((filters != null) && filters.contains(aFilter)))
	|| ((filters != null) && filters.contains(allFiltersgMatchThisString))
	|| (aFilter != null) && aFilter.equals("")) {
	    return true;
	} else {
	    return false;
	}
    }
    
    /**
    * Returns true if any of the following conditions are true:
    *   1. The specified filter is contained in the current filter set.
    *   2. The "ALL" filter is set (this is true even if the passed-in filter is "null").
    *	3. The passed-in filter is ""... as that filter is considered
    *      to <B>always</B> be a  match, regardless of the current filter set.
    *   4. The passed in filter is "null" and the "ALL" filter is currently set.
    * Otherwise returns false.
    *
    * NOTE: This method, and all filtering of this class is case-sensitive.
    */
    static public synchronized boolean containsFilter(String aFilter) {
	return Debug.containsFilter(aFilter, Debug.ALL_FILTER_STRING);
    }
    
    // Only used by the timing routines, since they ignore "ALL", but pay attention to
    // "TIMING".
    static private synchronized boolean containsTimingFilter(String aFilter) {
	return Debug.containsFilter(aFilter, Debug.TIMING_FILTER_STRING);
    }
        
    static private synchronized void maybeShowColumnHelp() {
	if (!showedColumnTitleHelp) {
	    Log.enableTimestampPrefix(false);
	    Log.logErrorMessage("************************:");
	    Log.logErrorMessage("Debugging time codes:");
	    Log.logErrorMessage("  etset=elapsed time since elapsed time (since the previous elapsedTime() call)");
	    Log.logErrorMessage("  etsst=elapsed time since start time (since the previous startTime() call)");
	    Log.logErrorMessage("  etsit=elapsed time since initialization time (typically since the program was launched)");
	    Log.logErrorMessage("************************:");
	    showedColumnTitleHelp = true;
	    Log.enableTimestampPrefix(false);
	}
    }

    // We don't use Log's debugging logging methods for a few reasons:
    // - They check for filters, but we've already had to check ourselves because of the
    //   "TIMING" filter, so we don't want to waste time checking again.
    // - It doesn't know about "TIMING", so if the filter was null, it would assume that
    //   "ALL" was set, not "TIMING".
    // - We need to add a suffix to the "baseName" of the filter that we deduce, and Log
    //   doesn't want to provide such a method ('cause it would be gross :-))
    // - We don't want the time and day stamp on each message, and though we could disable
    //   that around each call to Log's debugMessage stuff, that's pretty gross.
    static private synchronized void logTimingMessageString(String debugMessage, String debugFilter) {
	    String filterString;
	    
	    if (debugFilter == null) {
		if (Debug.containsFilter(Debug.TIMING_FILTER_STRING)) {
		    filterString = Debug.TIMING_FILTER_STRING;
		} else {
		    // Quiets compiler... If we're here, we should always be able to set it below.
		    filterString = "UNMATCHED FILTER";
		}
	    } else if (debugFilter.equals("")) {
		filterString = "ANY";
	    } else {
		filterString = debugFilter;
	    }
            String baseStr = Log.getApplicationName();
            if ( baseStr == null) {
                System.err.println("["+filterString+"]: "+ debugMessage);
		System.err.flush();
            }
            else {
                System.err.println(baseStr + "["+filterString+"]: "+ debugMessage);
		System.err.flush();
            }
    }

    /**
    * Starts a timer which can be stopped using one of the "stopTiming()" methods.
    *
    * This method does not check debug filters... it always does what it's told.
    *
    * Calling this method resets the elapsed time ("lap time").
    */
    static public synchronized void startTiming() {
	startTime = new Date().getTime();
	lapTime = startTime;
    }
    
    /**
    * Identical to the "startTiming(String logMessage, String aFilter)" method, except it
    * will only show the message if the filter "TIMING" exists in Debug's filter list.
    */
    static public synchronized void startTiming(String logMessage) {
	Debug.startTiming(logMessage, Debug.TIMING_FILTER_STRING);
    }
    
    /**
    * This method does absolutely nothing unless:
    *   1. The specified filter is contained in the current filter set.
    *   2. The "TIMING" filter is set (this is true even if the passed-in filter is "null").
    *	3. The passed-in filter is ""... as that filter is considered
    *      to <B>always</B> be a match, and so will cause this method to always work,
    *      regardless of the current filter set.
    *   NOTE: The "ALL" filter has no effect on timing methods.
    *
    * Otherwise, starts a timer which can be stopped using one of the "stopTiming()" methods
    * and prints out a logMessage indicating that timing has begun.
    *
    * This method may be called with a "null" logMessage. A logMessage is sometimes
    * unneccesary since the matched filter string is printed with the output anyway, and that
    * is frequently enough information.
    *
    * Calling this method resets the elapsed time ("lap time").
    */
    static public synchronized void startTiming(String logMessage, String aFilter) {
	if (Debug.containsTimingFilter(aFilter)) {
	    if (logMessage == null) {
		logMessage = "";
	    } else {
		logMessage = " ["+logMessage+"]";;
	    }
	    Debug.maybeShowColumnHelp();
	    Debug.startTiming();
	    Debug.logTimingMessageString("Resetting start time at etsit of: "+Debug.formatTime(Debug.elapsedTimeSinceInitialization())+logMessage, aFilter);
	}
    }
    
    static private synchronized long elapsedTime(long startTime) {
	lapTime = new Date().getTime();
	
	return lapTime - startTime; // "lapTime" happens to be currTime right now!
    }
    
    /**
    * Returns the elapsed time since this class was initialized.
    *
    * This method does not check debug filters... it always does what it's told.
    *
    * Calling this method resets the elapsed time ("lap time").
    *
    * This method may be called repeatedly to get "lap" times.
    */
    static public synchronized long elapsedTimeSinceInitialization() {
	return Debug.elapsedTime(initializationTime);
    }

    /**
    * Returns the elapsed time since the preceeding startTiming() call,
    *
    * This method does not check debug filters... it always does what it's told.
    *
    * Calling this method resets the elapsed time ("lap time").
    *
    * This method may be called repeatedly to get "lap" times.
    */
    static public synchronized long elapsedTimeSinceStartTime() {
	if (startTime == 0) {
	    return 0L; // "startTime()" was never called.
	} else {
	    return Debug.elapsedTime(startTime);
	}
    }

    /**
    * Returns the elapsed time since this class was initialized.
    *
    * This method does not check debug filters... it always does what it's told.
    *
    * Calling this method resets the elapsed time ("lap time").
    *
    * This method may be called repeatedly to get "lap" times.
    */
    static public synchronized long elapsedTimeSinceElapsedTime() {
	if (lapTime == 0) {
	    return 0L; // "elapsedTimeXXX()" was never called.
	} else {
	    return Debug.elapsedTime(lapTime);
	}
    }
    
    /**
    * Identical to the "elapsedTime(String logMessage, String aFilter)" method, except it
    * will only show the message if the filter "TIMING" exists in Debug's filter list.
    */
    static public synchronized void elapsedTime(String logMessage) {
	Debug.elapsedTime(logMessage, Debug.TIMING_FILTER_STRING);
    }

    /**
    * This method does absolutely nothing unless:
    *   1. The specified filter is contained in the current filter set.
    *   2. The "TIMING" filter is set (this is true even if the passed-in filter is "null").
    *	3. The passed-in filter is ""... as that filter is considered
    *      to <B>always</B> be a match, and so will cause this method to always work,
    *      regardless of the current filter set.
    *   NOTE: The "ALL" filter has no effect on timing methods.
    *
    * Otherwise, prints the following information:
    *     1. The elapsed time since initialization of this class.
    *     2. The elapsed time since the preceeding startTiming() call.
    *     3. The "lap" time, since the last time "elapsedTime()" was called.
    *     4. A client-supplied message.
    * 
    * When a filter matches, this method invokes the following methods:
    *	elapsedTime()
    *	elapsedTimeSinceInitialization()
    *	elapsedTimeSinceElapsedTime()
    *
    * When a filter matches, calling this method resets the elapsed time ("lap time").
    *
    *
    * This method may be called with a "null" logMessage. A logMessage is sometimes
    * unneccesary since the matched filter string is printed with the output anyway, and that
    * is frequently enough information.
    *
    * This method may be called repeatedly to get "lap" times.
    */
    static public synchronized void elapsedTime(String logMessage, String aFilter) {
	if (Debug.containsTimingFilter(aFilter)) {
	    if (logMessage == null) {
		logMessage = "";
	    } else {
		logMessage = " ["+logMessage+"]";;
	    }
	    Debug.maybeShowColumnHelp();
	    Debug.logTimingMessageString("etset: "+Debug.formatTime(Debug.elapsedTimeSinceElapsedTime())+" etsst: "+Debug.formatTime(Debug.elapsedTimeSinceStartTime())+" etsit: "+Debug.formatTime(Debug.elapsedTimeSinceInitialization())+logMessage, aFilter);
	}
    }
    
    /**
     * format end - start to sec.millisec
     */
    static private synchronized String formatTime(long milliseconds) {
        String d1000sStr = String.valueOf( milliseconds % 1000);
        int len = d1000sStr.length();
        return String.valueOf(milliseconds / 1000)+"."+ 
            "000".substring(len) + d1000sStr;
    }

    /**
     * format time to %4u%03u so that the decimal points will align
     */
    private static synchronized String formatTimeAligned(long milliseconds) {
        long deltaSecs =  milliseconds / 1000;
        String dSecsStr = null;
        if ( deltaSecs == 0)
            dSecsStr = "     ";
        else {
            dSecsStr = String.valueOf( deltaSecs);
            int len = dSecsStr.length();
            dSecsStr = "     ".substring( len) + dSecsStr;
        }

        String d1000sStr = String.valueOf( milliseconds % 1000);
        int len = d1000sStr.length();
        return dSecsStr + "." + "000".substring(len) + d1000sStr;
    }

    /**
    * Print time since start of app, and time since the last time this method
    * was called.
    * Call this with a msg you want printed, and a filter.  Then
    * run with -jsdebug filter and all these timing msgs will come out.
    */
    static public synchronized void printTime( String msg, String aFilter) {
        if ( filters != null && filters.contains( aFilter)) {
            long curTime = System.currentTimeMillis();
            System.out.println( formatTimeAligned( curTime - 
                                                   initializationTime) + "  " +
                                formatTimeAligned( curTime - lastTime) + "  " +
                                msg);
	    System.out.flush();
            lastTime = curTime;
        }
    }
       
    /**
    * Returns a String containing the hexadecimal hashCode of the passed in object,
    * of the form: "0x0000"
    */
    static public synchronized String getHashCode(Object anObject) {
	String returnValue;
	
	if (anObject == null) {
	    returnValue = "0x0000";
	} else {
	    returnValue = "0x"+Integer.toHexString(anObject.hashCode());
	}
	return returnValue;
    }
       
    /**
    * Returns a String containing the class name and hexadecimal hashCode of the
    * passed in object, of the form: "fully.qualified.ClassName[0x0000]"
    */
    static public synchronized String getNameAndHashCode(Object anObject) {
	String returnValue;
	
	if (anObject == null) {
	    returnValue = "<null>[0x0000]";
	} else {
	    returnValue = anObject.getClass().getName()+"["+Debug.getHashCode(anObject)+"]";
	}
	return returnValue;
    }
} // End of class Debug
