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

import java.io.FileOutputStream;
import java.io.BufferedOutputStream;
import java.io.PrintStream;
import java.io.IOException;

import org.w3c.dom.Document;
import org.w3c.dom.DocumentType;
import org.w3c.dom.Element;
import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.w3c.dom.*;

import org.mozilla.dom.test.*;

public class DOMFactory implements DocumentLoadListener {

  private static final boolean debug = false;

  public DocumentLoadListener getDocumentLoadListener() {
    return this;
  }

  public void startURLLoad(String url, String contentType, Document doc) {
    if (debug)
      System.err.println("DOM: start URL load - " + 
			 url.toString() + " " + 
			 contentType);
  }

  public void endURLLoad(String url, int status, Document doc) {


    if (debug)
      System.err.println("DOM: end URL load - " + 
			 url.toString() + " " +
			 Integer.toHexString(status));

    if (url.endsWith(".xul") || 
	url.endsWith(".js") || 
	url.endsWith(".css") || 
	url.startsWith("file:")) return;

   if ((!(url.endsWith(".html"))) && (!(url.endsWith(".xml"))))
     return;

   if (url.endsWith(".html"))
   {
     if (url.indexOf("test.html") == -1) {
       System.out.println("TestCases Tuned to run with test.html...");
       return;
     }
   }

   if (url.endsWith(".xml"))
   {
     if (url.indexOf("test.xml") == -1) {
       System.out.println("TestCases Tuned to run with test.xml...");
       return;
     }
   }

    Object obj = (Object) doc;

    TestLoader tl = new TestLoader(obj, 0);
    if (tl != null) {
       Object retobj = tl.loadTest();
    }

    doc = null;
  }

  public void progressURLLoad(String url, int progress, int progressMax, Document doc) {
    if (debug)
      System.err.println("DOM: progress URL load - " + 
			 url.toString() + " " +
			 Integer.toString(progress) + "/" +
			 Integer.toString(progressMax));
  }

  public void statusURLLoad(String url, String message, Document doc) {
    if (debug)
      System.err.println("DOM: status URL load - " + 
			 url.toString() + " (" +
			 message + ")");
  }

  public void startDocumentLoad(String url) {
    if (debug)
      System.err.println("DOM: start load - " + 
			 url.toString());
  }

  public void endDocumentLoad(String url, int status, Document doc) {
    if (debug) {
      System.err.println("DOM: end load - " + 
			 url.toString() + " " + 
			 Integer.toHexString(status));
    } 
  }

  private void dump(PrintStream ps, Node node, int indent, boolean mapNode) {
    ps.println();
    for (int i=0; i<indent; i++)
      ps.print(' ');

    ps.print("name=\"" + node.getNodeName() + "\"" + 
	     " type=" + dumpType(node.getNodeType()) +
	     " value=\"" + normalize(node.getNodeValue()) + "\"");

    if (mapNode)
	return;

    NamedNodeMap map = node.getAttributes();
    dumpMap(ps, map, indent);

    NodeList children = node.getChildNodes();
    if (children == null) 
      return;
    int length = children.getLength();
    for (int i=0; i < length; i++) {
      dump(ps, children.item(i), indent+2, false);
    }
  }

  private void dumpMap(PrintStream ps, NamedNodeMap map, int indent) {
    if (map == null) return;

    ps.println();
    int length = map.getLength();
    if (length > 0) {
      for (int i=0; i<indent; i++)
	ps.print(' ');
      ps.print("------- start attributes -------");
    }

    for (int i=0; i < length; i++) {
      Node item = map.item(i);
      dump(ps, item, indent, true);
    }

    if (length > 0) {
      ps.println();
      for (int i=0; i<indent; i++)
	ps.print(' ');
      ps.print("------- end attributes -------");
    }
  }

  private static String dumpType(int type) {
    switch (type) {
    case Node.ELEMENT_NODE: return "ELEMENT";
    case Node.ATTRIBUTE_NODE: return "ATTRIBUTE";
    case Node.TEXT_NODE: return "TEXT";
    case Node.CDATA_SECTION_NODE: return "CDATA_SECTION";
    case Node.ENTITY_REFERENCE_NODE: return "ENTITY_REFERENCE";
    case Node.ENTITY_NODE: return "ENTITY";
    case Node.PROCESSING_INSTRUCTION_NODE: return "PROCESSING_INSTRUCTION";
    case Node.COMMENT_NODE: return "COMMENT";
    case Node.DOCUMENT_NODE: return "DOCUMENT";
    case Node.DOCUMENT_TYPE_NODE: return "DOCUMENT_TYPE";
    case Node.DOCUMENT_FRAGMENT_NODE: return "DOCUMENT_FRAGMENT";
    case Node.NOTATION_NODE: return "NOTATION";
    }
    return "ERROR";
  }

  private static String normalize(String s) {
    int len = s.length();
    StringBuffer sb = new StringBuffer(len);
    char c = ' ';
    char pc = ' ';
    for (int i=0; i < len; i++) {
      c = s.charAt(i);
      if ((pc == ' ' || pc == '\n' || pc == '\t') &&
	  (c == ' ' || c == '\n' || c == '\t'))
	continue;
      sb.append(c);
      pc = c;
    }
    return sb.toString();
  }
}
