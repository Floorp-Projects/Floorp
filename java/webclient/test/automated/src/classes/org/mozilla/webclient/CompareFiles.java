/*
 * $Id: CompareFiles.java,v 1.4 2004/04/23 14:52:20 edburns%acm.org Exp $
 */

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun
 * Microsystems, Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): Ed Burns &lt;edburns@acm.org&gt;
 */

package org.mozilla.webclient;

import java.io.*;

import java.util.List;
import java.util.Iterator;

public class CompareFiles {

    public CompareFiles() {
    }

    /**
     * This method compares the input files character by character. 
     * Skips whitespaces and comparison is not case sensitive.
     */
    public static boolean filesIdentical (String actualFileName, 
					  String expectedFileName, 
					  List expectedLinesToIgnore, 
					  boolean ignorePrefix,
					  boolean ignoreWarnings, 
					  List ignoreKeywords) 
            throws IOException {

        boolean same = true;

        File actualFile = new File(actualFileName);
        File expectedFile = new File(expectedFileName);

        FileReader actualFileReader = new FileReader(actualFile);
        FileReader expectedFileReader = new FileReader(expectedFile);
	LineNumberReader actualReader = new LineNumberReader(actualFileReader);
	LineNumberReader expectedReader = new LineNumberReader(expectedFileReader);

	String actualLine, expectedLine;
	boolean swallowedLine = false;

	actualLine = actualReader.readLine().trim();
	expectedLine = expectedReader.readLine().trim();

	// if one of the lines is null, but not the other
	if (((null == actualLine) && (null != expectedLine)) ||
	    ((null != actualLine) && (null == expectedLine))) {
	    same = false;
	}

	while (null != actualLine && null != expectedLine) {
	    if (ignorePrefix) {
		int bracketColon = 0;
		if (-1 != (bracketColon = actualLine.indexOf("]: "))) {
		    actualLine = actualLine.substring(bracketColon + 3);
		}
		if (-1 != (bracketColon = expectedLine.indexOf("]: "))) {
		    expectedLine = expectedLine.substring(bracketColon + 3);
		}
	    }

	    swallowedLine = false;
	    // while the actual lines start with a warning condition
	    // keep reading them until we get a non-warning line or end
	    // of stream.
	    while (null != actualLine && ignoreWarnings && 
		   (-1 != actualLine.indexOf("WARNING:") || 
		    -1 != actualLine.indexOf("###!!! ASSERTION:") || 
		    -1 != actualLine.indexOf("###!!! Break:"))) {
		// we're ignoring warnings, no-op
		actualLine = actualReader.readLine(); // swallow WARNING
						      // line
		swallowedLine = true;
	    }
	    if (null != actualLine && swallowedLine) {
		continue;
	    }

	    swallowedLine = false;
	    // while the expected lines start with a warning condition,
	    // keep reading them until we get a non-warning line or end
	    // of stream.
	    while (null != expectedLine && ignoreWarnings && 
		   (-1 != expectedLine.indexOf("WARNING:") || 
		    -1 != expectedLine.indexOf("###!!! ASSERTION:") || 
		    -1 != expectedLine.indexOf("###!!! Break:"))) {
		// we're ignoring warnings, no-op
		expectedLine = expectedReader.readLine(); // swallow
		                                          // WARNING
		                                          // line
		swallowedLine = true;
	    }
	    if (null != actualLine && swallowedLine) {
		continue;
	    }

	    if (null == actualLine && null == expectedLine) {
		same = true;
		continue;
	    }
	    // if one of the lines is null, but not the other
	    if (((null == actualLine) && (null != expectedLine)) ||
		((null != actualLine) && (null == expectedLine))) {
		same = false;
		break;
	    }
	    if (!actualLine.equals(expectedLine)) {
		if (null != expectedLinesToIgnore) {
		    // go thru the list of expectedLinesToIgnore and see if
		    // the current expectedLine matches any of them.
		    Iterator ignoreLines = expectedLinesToIgnore.iterator();
		    boolean foundMatch = false;
		    while (ignoreLines.hasNext()) {
			String newTrim = ((String) ignoreLines.next()).trim();
			if (expectedLine.equals(newTrim)) {
			    foundMatch = true;
			    break;
			}
		    }
		    // If we haven't found a match, then this mismatch is
		    // important
		    if (!foundMatch) {
			same = false;
			System.out.println("actualLine: " + actualLine);
			System.out.println("expectedLine: " + expectedLine);
			break;
		    }
		}
		else {
		    same = false;
		    if (null != ignoreKeywords && 0 < ignoreKeywords.size()) {
			Iterator iter = ignoreKeywords.iterator();
			while (iter.hasNext()) {
			    if (-1 != actualLine.indexOf((String) iter.next())) {
				// we're ignoring lines that contain this
				// keyword, no-op
				same = true;
				break;
			    }
			}
		    }
		    if (!same) {
			System.out.println("actualLine: " + actualLine);
			System.out.println("expectedLine: " + expectedLine);
			break;
		    }
		}
	    }
	    
	    actualLine = actualReader.readLine();
	    expectedLine = expectedReader.readLine();

	    if (null != actualLine) {
		actualLine = actualLine.trim();
	    }
	    if (null != expectedLine) {
		expectedLine = expectedLine.trim();
	    }
	}

        actualReader.close();
        expectedReader.close();

        // if same is true and both files have reached eof, then
        // files are identical
        if (same == true) {
            return true;  
        } 
        return false;
    }
}
