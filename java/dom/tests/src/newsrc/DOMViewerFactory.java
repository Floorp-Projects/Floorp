/* 
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
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are Copyright (C) 1999 Sun Microsystems,
 * Inc. All Rights Reserved. 
 *
 * Contributor(s): Igor Kushnirskiy <idk@eng.sun.com>
 */

package org.mozilla.dom.test;

import org.mozilla.pluglet.*;
import org.mozilla.pluglet.mozilla.*;

import org.mozilla.dom.*;

import java.awt.print.*;
import java.awt.*;

import javax.swing.tree.*;
import javax.swing.event.*;
import javax.swing.*;
import java.util.*;

import org.w3c.dom.*;
import org.mozilla.dom.test.TestLoader;
import org.mozilla.dom.test.*;

public class DOMViewerFactory implements PlugletFactory {
    public DOMViewerFactory() {
    }

    public Pluglet createPluglet(String mimeType) {
 	return new DOMViewer();
    }

    public void initialize(PlugletManager manager) {	
    }

    public void shutdown() {
    }

}


class DOMViewer implements Pluglet {
    private PlugletPeer peer;
    private Node rootNode;

    public DOMViewer() {
    }
    
    public void initialize(PlugletPeer peer) {
      try {
	this.peer = peer;
	PlugletTagInfo info = peer.getTagInfo();
	if (info instanceof PlugletTagInfo2) {
	    PlugletTagInfo2 info2 = (PlugletTagInfo2)info;
	    Element e = (Element) info2.getDOMElement();
	    Document doc = e.getOwnerDocument();
	    e = doc.getDocumentElement();
	    //e.normalize();
	    //rootNode = e;
        
            // TestLoader hooks in here
            Object obj = (Object) doc;

            TestLoader tl = new TestLoader(obj, 0);
            if (tl != null) {
              Object retobj = tl.loadTest();
            }
	}

     } catch (Exception e) {
            e.printStackTrace();
     }

    }

    public void start() {
    }

    public void stop() {
    }

    public void destroy() {
    }

    public PlugletStreamListener newStream() {
	return null;
    }

    public void setWindow(Frame fr) {
    }

    public void print(PrinterJob printerJob) {
    }
}
