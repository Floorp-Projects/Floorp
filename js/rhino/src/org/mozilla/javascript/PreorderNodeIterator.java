/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Norris Boyd
 * Roger Lawrence
 * Igor Bukanov
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

package org.mozilla.javascript;

/**
 * This class implements a preorder tree iterator for the Node class.
 *
 * @see Node
 * @author Norris Boyd
 */
public class PreorderNodeIterator {
    public PreorderNodeIterator(Node n) {
        start = n;
    }

    public Node currentNode() {
        return current;
    }

    public Node getCurrentParent() {
        return currentParent;
    }

    public Node nextNode() {
        if (current == null) {
            current = start;
        }
        else if (current.first != null) {
            stackPush(current);
            currentParent = current;
            cachedPrev = null;
            current = current.first;
        }
        else {
            cachedPrev = current;
            current = current.next;
            while (current == null && stackTop != 0) {
                --stackTop;
                current = stack[stackTop];
                stack[stackTop] = null;

                cachedPrev = current;
                current = current.next;
            }
            currentParent = (current == null) ? null : stack[stackTop - 1];
        }
        return current;
    }

    public void replaceCurrent(Node newNode) {
        if (cachedPrev != null && cachedPrev.next == current) {
            // Check cachedPrev.next == current is necessary due to possible
            // tree mutations
            currentParent.replaceChildAfter(cachedPrev, newNode);
        }
        else {
            currentParent.replaceChild(current, newNode);
        }
        current = newNode;
    }
    
    private void stackPush(Node n) {
        int N = stackTop;
        if (N == 0) {
            stack = new Node[16];
        }
        else if (N == stack.length) {
            Node[] tmp = new Node[N * 2];
            System.arraycopy(stack, 0, tmp, 0, N);
            stack = tmp;
        }
        stack[N] = n;
        stackTop = N + 1;
    }

    private Node start;
    private Node[] stack;
    private int stackTop;

    private Node current;
    private Node currentParent;

//cache previous sibling of current not to search for it when
//replacing current
    private Node cachedPrev; 
}
