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
 * Contributor(s): Denis Sharypov <sdv@sparc.spb.su>
 *
 */

package org.mozilla.dom.util;

import java.io.BufferedOutputStream;
import java.io.ByteArrayOutputStream;
import java.io.PrintStream;
import java.io.FileOutputStream;
import java.io.IOException;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

public class DOMTreeDumper {

    private String name;
    private boolean debug;
    private PrintStream ps;
    private boolean inA;
    private final String[] endTagForbiddenNames = {"AREA",    
						   "BASE",    
						   "BASEFONT",
						   "BR",      
						   "COL",     
						   "FRAME",   
						   "HR",      
						   "IMG",     
						   "INPUT",   
						   "ISINDEX", 
						   "LINK",    
						   "META",    
						   "PARAM"};
    
    public DOMTreeDumper() {
	this("DOMTreeDumper", true);
    }

    public DOMTreeDumper(boolean debug) {
	this("DOMTreeDumper", debug);
    }

    public DOMTreeDumper(String name) {
	this(name, true);
    }

    public DOMTreeDumper(String name, boolean debug) {
	this.name = name;
	this.debug = debug;
    }

    private void dumpDocument(Document doc) {
	if (doc == null) return;
	Element element = doc.getDocumentElement();
	if (element == null) return;
	element.normalize();
//  	DocumentType dt = doc.getDoctype();
//  	dumpNode(dt);
	
	dumpNode(element);
	ps.println();
	ps.flush();
	
	element = null;
	doc = null;
    }

    private void dumpNode(Node node) {
	dumpNode(node, false);
    }

    private void dumpNode(Node node, boolean isMapNode) {
	if (node == null) {
	    return;
	}

	int type = node.getNodeType();
	String name = node.getNodeName();
	String value = node.getNodeValue();

	switch (type) {
	case Node.ELEMENT_NODE: 
	    if (name.equals("A")) inA = true;
	    if (!(inA || name.equals("BR"))) {
		ps.println();
	    }
	    ps.print("<" + name);
	    dumpAttributes(node);
	    ps.print(">");	
	    dumpChildren(node);
	    if (name.equals("A")) inA = false;
	    if (!endTagForbidden(name)) {
		ps.print("</" + node.getNodeName() + ">");	
	    }
	    break;
	case Node.ATTRIBUTE_NODE: 
	    ps.print(" " + name.toUpperCase() + "=\"" + value + "\"");
	    break;
	case Node.TEXT_NODE: 
	    if (!node.getParentNode().getNodeName().equals("PRE")) {
		value = value.trim();
	    }
	    if (!value.equals("")) {
		if (!inA) {
		    ps.println();
		}
		ps.print(canonicalize(value));
	    }
	    break;	
	case Node.COMMENT_NODE:
	    ps.print("\n<!--" + value + "-->");
	    break;
	case Node.CDATA_SECTION_NODE:
	case Node.ENTITY_REFERENCE_NODE:
	case Node.ENTITY_NODE:
	case Node.PROCESSING_INSTRUCTION_NODE:
	case Node.DOCUMENT_NODE:
	case Node.DOCUMENT_TYPE_NODE:
	case Node.DOCUMENT_FRAGMENT_NODE:
	case Node.NOTATION_NODE:
	    ps.println("\n<!-- NOT HANDLED: " + name + 
		       "  value=" + value + " -->");
	    break;
	}	
    }
    
    private void dumpAttributes(Node node) {
	NamedNodeMap map = node.getAttributes();
	if (map == null) return;
	int length = map.getLength();
	for (int i=0; i < length; i++) {
	    Node item = map.item(i);
	    dumpNode(item, true);
	}
    }
    
    private void dumpChildren(Node node) {
	NodeList children = node.getChildNodes();
	int length = 0;
	boolean hasChildren = ((children != null) && ((length = children.getLength()) > 0));
	if (!hasChildren) { 
	    return;
	}
	for (int i=0; i < length; i++) {
	    dumpNode(children.item(i), false);
	}
	if (!inA) {
	    ps.println();
	}
    }

    private String canonicalize(String str) {
	StringBuffer in = new StringBuffer(str);
	int length = in.length();
	StringBuffer out = new StringBuffer(length);
	char c;
	for (int i = 0; i < length; i++) {
	    switch (c = in.charAt(i)) {  			
	    case '&' :
		out.append("&amp;");
		break;
	    case '<':
		out.append("&lt;");
		break;
	    case '>':
		out.append("&gt;");
		break;
	    case '\u00A0':
		out.append("&nbsp;");
		break;
	    default:
		out.append(c);
	    }
	}
	return out.toString();
    }

    private boolean endTagForbidden(String name) {
	for (int i = 0; i < endTagForbiddenNames.length; i++) {
	    if (name.equals(endTagForbiddenNames[i])) {
		return true;
	    }
	}
	return false;
    }

    public void dumpToFile(String fileName, Document doc) {
	try {
	    FileOutputStream fos = new FileOutputStream(fileName);
	    ps = new PrintStream(new BufferedOutputStream(fos, 1024));
	} catch (IOException ex) {
	    ex.printStackTrace();
	    return;
	}	
	dbg("dumping to " + fileName);	
	dumpDocument(doc);
	dbg("finished dumping...");	
    }

    public void dumpToStream(PrintStream ps, Document doc) {
	this.ps = ps;
	dbg("dumping to stream...");	
	dumpDocument(doc);
	dbg("finished dumping...");	
    }
    
    public String dump(Document doc) {
        ByteArrayOutputStream baos = null;
        baos = new ByteArrayOutputStream(1024);
        ps = new PrintStream(baos);

        dbg("dumping to String");
        dumpDocument(doc);
        dbg("finished dumping...");
        return baos.toString();
    }

    
    private Element findElementWithName(Element start, String name) {
        Element result = null;
        Node child = null;
        String elementName = start.getAttribute("name");
        if (null != elementName && elementName.equals(name)) {
            return start;
        }
        else {
            NodeList children = start.getChildNodes();
            int length = 0;
            boolean hasChildren = ((children != null) && 
                    ((length = children.getLength()) > 0));
            if (!hasChildren) {
                return result;
            }
            for (int i=0; i < length; i++) {
                child = children.item(i);
                result = null;
                if (child instanceof Element) {
                    result = findElementWithName((Element) child, name);
                }
                if (null != result) {
                    break;
                }
            }
	}

        return result;
    }
    
    public Element findFirstElementWithName(Document doc, String name) {
        Element result = null;
        result = doc.getDocumentElement();
        if (null != result) {
            result.normalize();
            result = findElementWithName(result, name);
        }
        return result;
    }

    private void dbg(String str) {
	if (debug) {
	    System.out.println(name + ": " + str);
	}
    }
}
