/*
 * $Id: CompareFiles.java,v 1.1 2002/10/01 00:39:28 edburns%acm.org Exp $
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
    public static boolean filesIdentical (String newFileName, 
					  String oldFileName, 
					  List oldLinesToIgnore, 
					  int ignorePrefix) 
            throws IOException {

        boolean same = true;

        File newFile = new File(newFileName);
        File oldFile = new File(oldFileName);

        FileReader newFileReader = new FileReader(newFile);
        FileReader oldFileReader = new FileReader(oldFile);
	LineNumberReader newReader = new LineNumberReader(newFileReader);
	LineNumberReader oldReader = new LineNumberReader(oldFileReader);

	String newLine, oldLine;

	newLine = newReader.readLine().trim();
	oldLine = oldReader.readLine().trim();

	// if one of the lines is null, but not the other
	if (((null == newLine) && (null != oldLine)) ||
	    ((null != newLine) && (null == oldLine))) {
	    same = false;
	}

	while (null != newLine && null != oldLine) {
	    if (0 < ignorePrefix) {
		newLine = newLine.substring(ignorePrefix);
		oldLine = oldLine.substring(ignorePrefix);
	    }
	    
	    if (!newLine.equals(oldLine)) {

		if (null != oldLinesToIgnore) {
		    // go thru the list of oldLinesToIgnore and see if
		    // the current oldLine matches any of them.
		    Iterator ignoreLines = oldLinesToIgnore.iterator();
		    boolean foundMatch = false;
		    while (ignoreLines.hasNext()) {
			String newTrim = ((String) ignoreLines.next()).trim();
			if (oldLine.equals(newTrim)) {
			    foundMatch = true;
			    break;
			}
		    }
		    // If we haven't found a match, then this mismatch is
		    // important
		    if (!foundMatch) {
			same = false;
			break;
		    }
		}
		else {
		    same = false;
		    break;
		}
	    }
	    
	    newLine = newReader.readLine();
	    oldLine = oldReader.readLine();

	    // if one of the lines is null, but not the other
	    if (((null == newLine) && (null != oldLine)) ||
		((null != newLine) && (null == oldLine))) {
		same = false;
		break;
	    }
	    if (null != newLine) {
		newLine = newLine.trim();
	    }
	    if (null != oldLine) {
		oldLine = oldLine.trim();
	    }
	}

        newReader.close();
        oldReader.close();

        // if same is true and both files have reached eof, then
        // files are identical
        if (same == true) {
            return true;  
        } 
        return false;
    }
}
