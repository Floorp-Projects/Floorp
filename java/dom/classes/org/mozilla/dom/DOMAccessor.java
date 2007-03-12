/* 
 The contents of this file are subject to the Mozilla Public
 License Version 1.1 (the "License"); you may not use this file
 except in compliance with the License. You may obtain a copy of
 the License at http://www.mozilla.org/MPL/

 Software distributed under the License is distributed on an "AS
 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 implied. See the License for the specific language governing
 rights and limitations under the License.

 The Original Code is mozilla.org code.

 The Initial Developer of the Original Code is Sun Microsystems,
 Inc. Portions created by Sun are
 Copyright (C) 1999 Sun Microsystems, Inc. All
 Rights Reserved.

 Contributor(s): 
*/

/*

 * W3C® IPR SOFTWARE NOTICE

 * Copyright © 1994-2000 World Wide Web Consortium, (Massachusetts
 * Institute of Technology, Institut National de Recherche en
 * Informatique et en Automatique, Keio University). All Rights
 * Reserved. http://www.w3.org/Consortium/Legal/

 * This W3C work (including software, documents, or other related items) is
 * being provided by the copyright holders under the following
 * license. By obtaining, using and/or copying this work, you (the
 * licensee) agree that you have read, understood, and will comply with
 * the following terms and conditions:

 * Permission to use, copy, and modify this software and its documentation,
 * with or without modification, for any purpose and without fee or
 * royalty is hereby granted, provided that you include the following on
 * ALL copies of the software and documentation or portions thereof,
 * including modifications, that you make:

 * The full text of this NOTICE in a location viewable to users of the
 * redistributed or derivative work.

 * Any pre-existing intellectual property disclaimers, notices, or terms
 * and conditions. If none exist, a short notice of the following form
 * (hypertext is preferred, text is permitted) should be used within the
 * body of any redistributed or derivative code: "Copyright ©
 * [$date-of-software] World Wide Web Consortium, (Massachusetts
 * Institute of Technology, Institut National de Recherche en
 * Informatique et en Automatique, Keio University). All Rights
 * Reserved. http://www.w3.org/Consortium/Legal/"

 * Notice of any changes or modifications to the W3C files, including
 * the date changes were made. (We recommend you provide URIs to the
 * location from which the code is derived.)

 * THIS SOFTWARE AND DOCUMENTATION IS PROVIDED "AS IS," AND COPYRIGHT
 * HOLDERS MAKE NO REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO, WARRANTIES OF MERCHANTABILITY OR
 * FITNESS FOR ANY PARTICULAR PURPOSE OR THAT THE USE OF THE SOFTWARE OR
 * DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS, COPYRIGHTS,
 * TRADEMARKS OR OTHER RIGHTS.

 * COPYRIGHT HOLDERS WILL NOT BE LIABLE FOR ANY DIRECT, INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF ANY USE OF THE
 * SOFTWARE OR DOCUMENTATION.

 * The name and trademarks of copyright holders may NOT be used in
 * advertising or publicity pertaining to the software without specific,
 * written prior permission. Title to copyright in this software and any
 * associated documentation will at all times remain with copyright
 * holders.

 */


package org.mozilla.dom;

import java.util.Vector;
import java.util.Enumeration;

import org.w3c.dom.Document;
import org.w3c.dom.Node;

import java.security.AccessController;

public final class DOMAccessor {

    private static Vector documentLoadListeners = new Vector();
    private static JavaDOMPermission permission = new JavaDOMPermission("JavaDOM");

    static {
	System.loadLibrary("javadomjni");
    }

    private void DOMAccessorImpl() {}

    private static void register() {
	nativeRegister();
    }
    private static native void nativeRegister();

    private static void unregister() {
	nativeUnregister();
    }
    private static native void nativeUnregister();
    
    public static Node getNodeByHandle(long p) {
	return nativeGetNodeByHandle(p);
    }
    static native Node nativeGetNodeByHandle(long p);

    private static void doGC() {
	nativeDoGC();
    }
    private static native void nativeDoGC();

    public static void initialize() {
	nativeInitialize();
    }
    public static native void nativeInitialize();

    public static synchronized void 
	addDocumentLoadListener(DocumentLoadListener listener) {
	if (documentLoadListeners.size() == 0) {
	    register();
	}
	documentLoadListeners.addElement(listener);
    }

    public static synchronized void 
	removeDocumentLoadListener(DocumentLoadListener listener) {

	documentLoadListeners.removeElement(listener);
	if (documentLoadListeners.size() == 0) {
	  unregister();
	}
    }

    public static synchronized void 
        startURLLoad(String url, String contentType, long p_doc) {

	AccessController.checkPermission(permission);
	for (Enumeration e = documentLoadListeners.elements();
	     e.hasMoreElements();) {
	    DocumentLoadListener listener = 
		(DocumentLoadListener) e.nextElement();
	    listener.startURLLoad(url, contentType, (Document)getNodeByHandle(p_doc));
	}
	doGC();
    }

    public static synchronized void 
	endURLLoad(String url, int status, long p_doc) {

	AccessController.checkPermission(permission);
	for (Enumeration e = documentLoadListeners.elements();
	     e.hasMoreElements();) {
	    DocumentLoadListener listener = 
		(DocumentLoadListener) e.nextElement();
	    listener.endURLLoad(url, status, (Document)getNodeByHandle(p_doc));
	}
	doGC();
    }

    public static synchronized void 
	progressURLLoad(String url, int progress, int progressMax, 
			long p_doc) {

	AccessController.checkPermission(permission);
	for (Enumeration e = documentLoadListeners.elements();
	     e.hasMoreElements();) {
	    DocumentLoadListener listener = 
		(DocumentLoadListener) e.nextElement();
	    listener.progressURLLoad(url, progress, progressMax, (Document)getNodeByHandle(p_doc));
	}
	doGC();
    }

    public static synchronized void 
        statusURLLoad(String url, String message, long p_doc) {

	AccessController.checkPermission(permission);
	for (Enumeration e = documentLoadListeners.elements();
	     e.hasMoreElements();) {
	    DocumentLoadListener listener = 
		(DocumentLoadListener) e.nextElement();
	    listener.statusURLLoad(url, message, (Document)getNodeByHandle(p_doc));
	}
	doGC();
    }


    public static synchronized void 
        startDocumentLoad(String url) {

	AccessController.checkPermission(permission);
	for (Enumeration e = documentLoadListeners.elements();
	     e.hasMoreElements();) {
	    DocumentLoadListener listener = 
		(DocumentLoadListener) e.nextElement();
	    listener.startDocumentLoad(url);
	}
	doGC();
    }

    public static synchronized void 
        endDocumentLoad(String url, int status, long p_doc) {

	AccessController.checkPermission(permission);
	for (Enumeration e = documentLoadListeners.elements();
	     e.hasMoreElements();) {
	    DocumentLoadListener listener = 
		(DocumentLoadListener) e.nextElement();
	    listener.endDocumentLoad(url, status, (Document)getNodeByHandle(p_doc));
	}
	doGC();
    }
}
