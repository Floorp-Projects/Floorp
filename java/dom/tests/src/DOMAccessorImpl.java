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

import org.mozilla.dom.test.*;

public class DOMAccessorImpl implements DOMAccessor, DocumentLoadListener {

    private static DOMAccessor instance = null;
    private Vector documentLoadListeners = new Vector();

    public static native void register();
    public static native void unregister();

    static {
	System.err.println("\n\nDOMAccessor: autoregestring ...\n\n");
	try {
		//at this moment library is not loaded yet - tet's do it
		System.loadLibrary("javadomjni");
	} catch (Exception e) {
		System.out.println("Can't load javadomjni.dll: "+e);
		System.exit(-1);
	}
	DOMAccessorImpl.getInstance().addDocumentLoadListener(new TestListener());
    }

    public static synchronized DOMAccessor getInstance() {
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

	System.err.println("############## DOMAccessor: startURLLoad "+url);

	for (Enumeration e = documentLoadListeners.elements();
	     e.hasMoreElements();) {
	    DocumentLoadListener listener = 
		(DocumentLoadListener) e.nextElement();
	    listener.startURLLoad(url, contentType, doc);
	}
    }

    public synchronized void 
	endURLLoad(String url, int status, Document doc) {

	System.err.println("############ DOMAccessor: endURLLoad "+url);


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


	System.err.println("############ DOMAccessor: progressURLLoad "+url);

	for (Enumeration e = documentLoadListeners.elements();
	     e.hasMoreElements();) {
	    DocumentLoadListener listener = 
		(DocumentLoadListener) e.nextElement();
	    listener.progressURLLoad(url, progress, progressMax, doc);
	}
    }

    public synchronized void 
        statusURLLoad(String url, String message, Document doc) {


	System.err.println("############ DOMAccessor: statusURLLoad "+url);

	for (Enumeration e = documentLoadListeners.elements();
	     e.hasMoreElements();) {
	    DocumentLoadListener listener = 
		(DocumentLoadListener) e.nextElement();
	    listener.statusURLLoad(url, message, doc);
	}
    }


    public synchronized void 
        startDocumentLoad(String url) {


	System.err.println("############ DOMAccessor: startDocLoad "+url);

	for (Enumeration e = documentLoadListeners.elements();
	     e.hasMoreElements();) {
	    DocumentLoadListener listener = 
		(DocumentLoadListener) e.nextElement();
	    listener.startDocumentLoad(url);
	}
    }

    public synchronized void 
        endDocumentLoad(String url, int status, Document doc) {


	System.err.println("############ DOMAccessor: endDocLoad "+url);

	for (Enumeration e = documentLoadListeners.elements();
	     e.hasMoreElements();) {
	    DocumentLoadListener listener = 
		(DocumentLoadListener) e.nextElement();
	    listener.endDocumentLoad(url, status, doc);
	}
    }
} //end of class



class TestListener implements DocumentLoadListener {

    public void endDocumentLoad(String url, int status, Document doc) {

      if (url.endsWith(".xul")
  	|| url.endsWith(".js")
	|| url.endsWith(".css") 
	//|| url.startsWith("file:")
	) 
	return;

   if ((!(url.endsWith(".html"))) && (!(url.endsWith(".xml"))))
     return;

   if (url.endsWith(".html"))
   {
     if (url.indexOf("test.html") == -1) {
       System.err.println("TestCases Tuned to run with test.html...");
       return;
     }
   }

   if (url.endsWith(".xml"))
   {
     if (url.indexOf("test.xml") == -1) {
       System.err.println("TestCases Tuned to run with test.xml...");
       return;
     }
   }

    Object obj = (Object) doc;
System.out.println("Loading new TestLoader...\n");

    TestLoader tl = new TestLoader(obj, 0);
    if (tl != null) {
       Object retobj = tl.loadTest();
    }

    //doc = null;


    }

    public void startURLLoad(String url, String contentType, Document doc) {}
    public void progressURLLoad(String url, int progress, int progressMax, 
				Document doc) {}
    public void statusURLLoad(String url, String message, Document doc) {}

    public void startDocumentLoad(String url) {}
    public void endURLLoad(String url, int status, Document doc) {}

} //end of class
