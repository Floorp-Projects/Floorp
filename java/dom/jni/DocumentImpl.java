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

import org.w3c.dom.Attr;
import org.w3c.dom.CDATASection;
import org.w3c.dom.Comment;
import org.w3c.dom.Document;
import org.w3c.dom.DocumentFragment;
import org.w3c.dom.Element;
import org.w3c.dom.EntityReference;
import org.w3c.dom.ProcessingInstruction;
import org.w3c.dom.Text;
import org.w3c.dom.DocumentType;
import org.w3c.dom.NodeList;
import org.w3c.dom.DOMImplementation;

public class DocumentImpl extends NodeImpl implements Document {

    // instantiated from JNI only
    private DocumentImpl() {}

    public native Attr createAttribute(String name);
    public native CDATASection createCDATASection(String data);
    public native Comment createComment(String data);
    public native DocumentFragment createDocumentFragment();
    public native Element createElement(String tagName);
    public native EntityReference createEntityReference(String name);
    public native ProcessingInstruction 
	createProcessingInstruction(String target, 
				    String data);
    public native Text createTextNode(String data);

    public native DocumentType getDoctype();
    public native Element getDocumentElement();
    public native NodeList getElementsByTagName(String tagName);
    public native DOMImplementation getImplementation();

    protected native void finalize();
    private static native void initialize();

    static {
	System.loadLibrary("javadomjni");
	initialize();
    }
}
