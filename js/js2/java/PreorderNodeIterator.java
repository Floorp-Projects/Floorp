/* -*- Mode: java; tab-width: 8 -*-
 * Copyright © 1997, 1998 Netscape Communications Corporation, All Rights Reserved.
 */

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
