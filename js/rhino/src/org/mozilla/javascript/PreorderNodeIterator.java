/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package org.mozilla.javascript;

import java.util.Stack;

/**
 * This class implements a preorder tree iterator for the Node class.
 *
 * @see Node
 * @author Norris Boyd
 */
public class PreorderNodeIterator {
    public PreorderNodeIterator(Node n) {
    	start = n;
    	stack = new Stack();
    }

    public Node currentNode() {
        return current;
    }

    public Node getCurrentParent() {
    	return currentParent;
    }

    public Node nextNode() {
        if (current == null)
            return current = start;
    	if (current.first != null) {
    	    stack.push(current);
    	    currentParent = current;
    	    current = current.first;
    	} else {
    	    current = current.next;
    	    boolean isEmpty;
            for (;;) {
                isEmpty = stack.isEmpty();
                if (isEmpty || current != null)
    		        break;
                current = (Node) stack.pop();
                current = current.next;
    	    }
    	    currentParent = isEmpty ? null : (Node) stack.peek();
    	}
    	return current;
    }

    public void replaceCurrent(Node newNode) {
        currentParent.replaceChild(current, newNode);
        current = newNode;
    }

    private Node start;
    private Node current;
    private Node currentParent;
    private Stack stack;
}
