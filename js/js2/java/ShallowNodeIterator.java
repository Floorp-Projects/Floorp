/* -*- Mode: java; tab-width: 8 -*-
 * Copyright © 1997, 1998 Netscape Communications Corporation, All Rights Reserved.
 */

import java.util.Enumeration;

/**
 * This class implements a child iterator for the Node class.
 *
 * @see Node
 * @author Norris Boyd
 */
class ShallowNodeIterator implements Enumeration {

    public ShallowNodeIterator(Node n) {
        current = n;
    }

    public boolean hasMoreElements() {
        return current != null;
    }

    public Object nextElement() {
        return nextNode();
    }

    public Node nextNode() {
        Node result = current;
        current = current.next;
        return result;
    }

    private Node current;
}

