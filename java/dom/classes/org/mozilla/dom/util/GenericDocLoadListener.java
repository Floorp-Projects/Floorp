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

package org.mozilla.dom.util;

import java.io.PrintStream;
import org.w3c.dom.Document;
import org.mozilla.dom.DocumentLoadListener;

public class GenericDocLoadListener implements DocumentLoadListener {

    private String name;
    private PrintStream ps;

    public GenericDocLoadListener() {
	this("GenericDocLoadListener", 
	     new PrintStream(System.out));
    }

    public GenericDocLoadListener(String name) {
	this(name, 
	    new PrintStream(System.out));
    }

    public GenericDocLoadListener(PrintStream ps) {
	this("GenericDocLoadListener", ps);
    }

    public GenericDocLoadListener(String name, PrintStream ps) {
	this.name = name;
	this.ps = ps;	
    }

    public DocumentLoadListener getDocumentLoadListener() {
	return this;
    }

    public void startURLLoad(String url, String contentType, Document doc) {
	if (ps != null) {
	    ps.println(name + " :start URL load - " + 
			       url.toString() + " " + 
			       contentType);
	}
    }

    public void endURLLoad(String url, int status, Document doc) {
	
	if (ps != null) {
	    ps.println(name + " :end URL load - " + 
			       url.toString() + " " +
			       Integer.toHexString(status));
	}
    }
    
    public void progressURLLoad(String url, int progress, int progressMax,
				Document doc) {
	if (ps != null) {
	    ps.println(name + " :progress URL load - " + 
			       url.toString() + " " +
			       Integer.toString(progress) + "/" +
			       Integer.toString(progressMax));
	}
    }
    
    public void statusURLLoad(String url, String message, Document doc) {
	if (ps != null) {
	    ps.println(name + " :status URL load - " + 
			       url.toString() + " (" +
			       message + ")");
	}
    }

    public void startDocumentLoad(String url) {
	if (ps != null) {
	    ps.println(name + " :start doc load - " + 
			       url.toString());
	}
    }

    public void endDocumentLoad(String url, int status, Document doc) {
	if (ps != null) {
	    ps.println(name + " :end doc load - " + 
			       url.toString() + " " + 
			       Integer.toHexString(status));
	} 
    }
}
