/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Patrick C. Beard <beard@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

import java.util.Enumeration;
import java.util.Vector;

/**
 * Represents a global tree of calls.
 */
public class CallTree {
	public static class Node {
	    int id;
	    Node parent;
	    String data;
	    
	    public Node(int id, Node parent) {
	        this.id = id;
	        this.parent = parent;
	    }
	};

	private Vector nodes = new Vector(16000);
	private StringTable strings;

    public CallTree(StringTable strings) {
        nodes.addElement(null);
        this.strings = strings;
    }
	
	/**
	 * Parses a line of the form:
	 * <c id=number pid=number>function[file,offset]</c>
	 */
	public Node parseNode(String line) {
        try {
            int start = 1 + line.indexOf('=');
            int endTag = line.indexOf('>');
            if (line.charAt(endTag - 1) == '/') {
                // just references an already parsed node.
                int id = Integer.parseInt(line.substring(start, endTag - 1));
                return (Node) nodes.elementAt(id);
            } else {
                int end = line.indexOf(' ', start + 1);
                int id = Integer.parseInt(line.substring(start, end));
                start = 1 + line.indexOf('=', end + 1);
                end = line.indexOf('>', start + 1);
                int pid = Integer.parseInt(line.substring(start, end));
                Node node = addNode(id, pid);
                start = end + 1;
                end = line.lastIndexOf('<');
                node.data = strings.intern(line.substring(start, end));
                return node;
            }
        } catch (NumberFormatException nfe) {
        }
        return null;
	}
	
	public Node addNode(int id, int pid) {
	    Node parent = (Node) nodes.elementAt(pid);
	    Node node = new Node(id, parent);
	    nodes.addElement(node);
	    if (nodes.size() != (id + 1))
	        throw new Error("inconsistent node id");
	    return node;
	}
	
	public Node getNode(int id) {
	    return (Node) nodes.elementAt(id);
	}
}
