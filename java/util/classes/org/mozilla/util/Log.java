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

import java.util.Date;

/**
 * <P>
 * <B>Log</B>
 * </P>
 * @author Keith Bernstein
 * @version $Id: Log.java,v 1.1 1999/07/30 01:02:57 edburns%acm.org Exp $
 */

public class Log extends Object {
    static String applicationName = "APPLICATION NAME UNKNOWN [call setApplicationName() from main]";
    static String applicationVersion = "APPLICATION VERSION UNKNOWN [call setApplicationVersion() from main]";
    static String applicationVersionDate = "APPLICATION VERSION DATE UNKNOWN [call setApplicationVersionDate() from main]";
    static int showTimestampPrefix = 1;
    
    /**
    * This string will be prepended to all output from this class.
    *
    * This string is usually the application name, e.g. "JavaPlan".
    *
    * It is useful because it includes a timestamp and the base string (usually the
    * application name), so that if two apps are launched from the same commandline
    * (for example), with "&", or one app invokes another, it is clear who the message
    * is comming from. The time stamp can help see how long operations took, etc.
    *
    * If you don't want your messages prefixed with anything (no time stamp or name), you must
    * pass "null" for "applicationName".
    *
    * If this method is never called, "applicationName" will default to:
    *    "APPLICATION NAME UNKNOWN [call setApplicationName from main]"
    * 
    */
    static public synchronized void setApplicationName(String newApplicationName) {
	// It's really unfortunate that we can't discover this dynamically.
	applicationName = newApplicationName;
    }
    
    /**
    * Returns the applicationName set by "setApplicationName()"
    *
    */
    static public synchronized String getApplicationName() {
	// It's really unfortunate that we can't discover this dynamically.
	return applicationName;
    }
    
    static public synchronized void setApplicationVersion(String newApplicationVersion) {
	// It's really unfortunate that we can't discover this dynamically.
	applicationVersion = newApplicationVersion;
    }
    
    /**
    * Returns the applicationVersion set by "setApplicationVersion()"
    *
    */
    static public synchronized String getApplicationVersion() {
	// It's really unfortunate that we can't discover this dynamically.
	return applicationVersion;
    }
    
    static public synchronized void setApplicationVersionDate(String newApplicationVersionDate) {
	// It's really unfortunate that we can't discover this dynamically.
	applicationVersionDate = newApplicationVersionDate;
    }
    
    /**
    * Returns the applicationVersion set by "setApplicationVersionDate()"
    *
    */
    static public synchronized String getApplicationVersionDate() {
	// It's really unfortunate that we can't discover this dynamically.
	return applicationVersionDate;
    }
    
    static protected synchronized Object applicationNameWithTimeStamp() {
	String timestampPrefix;
	
	if (showTimestampPrefix > 0) {
	    timestampPrefix = "["+new Date()+"] ";
	} else {
	    timestampPrefix = "";
	}
	if (applicationName != null) {
	    return timestampPrefix+Log.getApplicationName();
	} else {
	    return timestampPrefix;
	}
    }
    
    /**
    * Incrememnts or decrements whether or not to prefix logged messages with a timestamp.
    * Two (or "n") calls with a value of "false" must be followed by two (or "n") calls
    * with a value of "true" to reenable timestamp prefixes.
    */
    static public synchronized void enableTimestampPrefix(boolean enable) {
	if (enable) {
	    showTimestampPrefix++;
	} else {
	    showTimestampPrefix--;
	}
    }

    /**
    * Writes "infoMessage" to stdout, prefixed by the string "ApplicationName: "
    */
    static public synchronized void log(Object infoMessage) {
	System.out.println(Log.applicationNameWithTimeStamp()+": "+infoMessage);
        System.out.flush();
    }
    
    /**
    * Writes "errorMessage" to stderr, prefixed by the string "ApplicationName error: "
    */
    static public synchronized void logError(Object errorMessage) {
	System.err.println(Log.applicationNameWithTimeStamp()+" error: "+errorMessage);
        System.err.flush();
    }
    
    /**
    * Writes "errorMessage" to stderr, prefixed by the string "ApplicationName: "
    */
    static public synchronized void logErrorMessage(Object errorMessage) {
	System.err.println(Log.applicationNameWithTimeStamp()+": "+errorMessage);
        System.err.flush();
    }    
    
    static private synchronized void logErrorMessage(String baseNameSuffixString, Object errorMessage) {
	System.err.println(Log.applicationNameWithTimeStamp()+baseNameSuffixString+": "+errorMessage);
        System.err.flush();
    }    
    
    /**
    * Funnel-point method for printing debug messages.
    *
    * Writes "debugMessage" to stderr, prefixed by the string "ApplicationName:"
    *
    * This method only works if the debugFilter string is found in Debug's list of
    * filter strings (which you can normally set  on the commandline, see JDApplication),
    * or if the "ALL" filter has been set into Debug's list of filters, or if the passed
    * in filter is "", which is considered to always be "set", and will print a line with
    * "[DEBUG]" listed as the filter.
    *
    * This method may be called with a "null" debugMessage. A debugMessage is sometimes
    * unneccesary since the matched filter string is printed with the output anyway, and that
    * is frequently enough information.
   */
    static public synchronized void logDebugMessage(Object debugMessage, String debugFilter) {
    	if (Debug.containsFilter(debugFilter)) {
	    String filterString;
	    
	    if (debugFilter == null) {
		if (Debug.containsFilter(Debug.ALL_FILTER_STRING)) {
		    filterString = Debug.ALL_FILTER_STRING;
		} else {
		    // Quiets compiler... If we're here, we should always be able to set it below.
		    filterString = "UNMATCHED FILTER";
		}
	    } else if (debugFilter.equals("")) {
		filterString = "DEBUG";
	    } else {
		filterString = debugFilter;
	    }
    	    Log.logErrorMessage("["+filterString+"]", debugMessage);
	}
    }   
     
    /**
    * Equivalent to calling "logDebugMessage(debugMessage, "ALL")".
    */
    static public synchronized void logDebugMessage(Object debugMessage) {
	logDebugMessage(debugMessage, "ALL");
    }   

    /**
    * Log a message when "aCondition" is true, otherwise be silent.
    * Equivalent to calling "if (aCondition) logDebugMessage(debugMessage, "")".
    */
    static public synchronized void logDebugMessage(Object debugMessage, boolean aCondition) {
	if (aCondition) {
	    Log.logDebugMessage(debugMessage, "");
	}
    }   

    /**
    * Log a message when "aCondition" is true, otherwise be silent.
    */
    static public synchronized void logDebugMessage(Object anInstance, Object debugMessage, boolean aCondition) {
	if (aCondition) {
	    Log.logDebugMessage(Debug.getNameAndHashCode(anInstance)+": "+debugMessage, "");
	}
    }   

    /**
    * Equivalent to calling "logDebugMessage(anInstance, debugMessage, "ALL")".
    */
    static public synchronized void logDebugMessage(Object anInstance, Object debugMessage) {
	Log.logDebugMessage(anInstance, debugMessage, "ALL");
    }   

    /**
    * Writes "debugMessage" to stderr, prefixed by the string "ApplicationName: ClassName[0xhashCode]: "    *
    */
    static public synchronized void logDebugMessage(Object anInstance, Object debugMessage, String debugFilter) {
	Log.logDebugMessage(Debug.getNameAndHashCode(anInstance)+": "+debugMessage, debugFilter);
    }    
} // End of class Log
