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

package org.mozilla.dom.tests;

import java.util.Vector;
import java.applet.Applet;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.w3c.dom.Document;
import org.w3c.dom.events.EventListener;
import org.w3c.dom.events.EventTarget;
import org.mozilla.dom.DOMAccessor;
import org.mozilla.dom.NodeImpl;
import org.mozilla.dom.util.GenericEventListener;
import org.mozilla.dom.util.GenericDocLoadListener;

public class TestDOMEventsApplet extends Applet {

    private final String name = "TestDOMEventsApplet";

    public void init()
    {
        DOMAccessor.addDocumentLoadListener(new TestDOMEventsListener(name));
	System.out.println(name + " init...");
    }

    class TestDOMEventsListener extends GenericDocLoadListener {
	public TestDOMEventsListener(String name) {
	    super(name);
	}

	public void endDocumentLoad(String url, int status, Document doc) {
	    if (url.endsWith(".html")){
		Vector v;
		Object o;
		Node n; 
		NodeList l; 
		int i;
		EventListener listener; 
		NodeImpl target;
		String [] types = {"mouseover", "dblclick", "mouseout", "keydown", "keyup",
				   "mousedown", "mouseup", "click", "keypress",
				   "mousemove", "focus", "blur", "submit", "reset",
				   "change", "select", "input", "load", "unload",
				   "abort", "error", "paint", "create", "destroy",
				   "command", "broadcast", "commandupdate", 
				   "dragenter", "dragover", "dragexit", "dragdrop",
				   "draggesture"};

		listener = new GenericEventListener("name");
		v = new Vector (20, 10);
		v.add(doc);
		
		while(!v.isEmpty()) {
		    o = v.elementAt(0);
		    if (o instanceof Node) {
			n = (Node) o;
			if (n instanceof NodeImpl) 
			    for(i=0; i < types.length; i++) {
				target = (NodeImpl) n;
				target.addEventListener(types[i], listener, false);
			    }
			
			l = n.getChildNodes();
			for(i=0; i<l.getLength(); i++) 
			    v.add(l.item(i));
		    } 
		    v.remove(0);
		}
	    }
	}
    }
}
