/* 
The contents of this file are subject to the Mozilla Public License
Version 1.0 (the "License"); you may not use this file except in
compliance with the License. You may obtain a copy of the License at
http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS"
basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
the License for the specific language governing rights and limitations
under the License.

The Initial Developer of the Original Code is Sun Microsystems,
Inc. Portions created by Sun are Copyright (C) 1999 Sun Microsystems,
Inc. All Rights Reserved. 
*/

package org.mozilla.dom;

import java.util.Vector;
import java.util.Enumeration;

import org.w3c.dom.Document;

public class DOMAccessorImpl implements DOMAccessor, DocumentLoadListener {

    private DOMAccessor instance = null;
    private Vector documentLoadListeners = new Vector();

    public static native void register();
    public static native void unregister();

    public synchronized DOMAccessor getInstance() {
	if (instance == null) {
	    instance = new DOMAccessorImpl();
	}
	return instance;
    }

    public synchronized void 
	addDocumentLoadListener(DocumentLoadListener listener) {

	if (documentLoadListeners.size() == 0) {
	  register();
	}
	documentLoadListeners.addElement(listener);
    }

    public synchronized void 
	removeDocumentLoadListener(DocumentLoadListener listener) {

	documentLoadListeners.removeElement(listener);
	if (documentLoadListeners.size() == 0) {
	  unregister();
	}
    }

    public synchronized void 
        startURLLoad(String url, String contentType, Document doc) {

	for (Enumeration e = documentLoadListeners.elements();
	     e.hasMoreElements();) {
	    DocumentLoadListener listener = 
		(DocumentLoadListener) e.nextElement();
	    listener.startURLLoad(url, contentType, doc);
	}
    }

    public synchronized void 
	endURLLoad(String url, int status, Document doc) {

	for (Enumeration e = documentLoadListeners.elements();
	     e.hasMoreElements();) {
	    DocumentLoadListener listener = 
		(DocumentLoadListener) e.nextElement();
	    listener.endURLLoad(url, status, doc);
	}
    }

    public synchronized void 
	progressURLLoad(String url, int progress, int progressMax, 
			Document doc) {

	for (Enumeration e = documentLoadListeners.elements();
	     e.hasMoreElements();) {
	    DocumentLoadListener listener = 
		(DocumentLoadListener) e.nextElement();
	    listener.progressURLLoad(url, progress, progressMax, doc);
	}
    }

    public synchronized void 
        statusURLLoad(String url, String message, Document doc) {

	for (Enumeration e = documentLoadListeners.elements();
	     e.hasMoreElements();) {
	    DocumentLoadListener listener = 
		(DocumentLoadListener) e.nextElement();
	    listener.statusURLLoad(url, message, doc);
	}
    }


    public synchronized void 
        startDocumentLoad(String url) {

	for (Enumeration e = documentLoadListeners.elements();
	     e.hasMoreElements();) {
	    DocumentLoadListener listener = 
		(DocumentLoadListener) e.nextElement();
	    listener.startDocumentLoad(url);
	}
    }

    public synchronized void 
        endDocumentLoad(String url, int status, Document doc) {

	for (Enumeration e = documentLoadListeners.elements();
	     e.hasMoreElements();) {
	    DocumentLoadListener listener = 
		(DocumentLoadListener) e.nextElement();
	    listener.endDocumentLoad(url, status, doc);
	}
    }
}
