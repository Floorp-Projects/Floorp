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

