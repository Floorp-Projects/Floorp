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

    private static native void register();
    private static native void unregister();
    private static native Node getNodeByHandle(long p);
    private static native void doGC();

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
