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
 * Contributor(s): Igor Kushnirskiy <idk@eng.sun.com>
 *      Ed Burns <edburns@acm.org>
 *      Jason Mawdsley <jason@macadamian.com>
 *      Louis-Philippe Gagnon <louisphilippe@macadamian.com>
 */

package org.mozilla.dom.util;

import javax.swing.tree.TreePath;
import javax.swing.tree.TreeModel;
import javax.swing.event.TreeModelEvent;
import javax.swing.event.TreeModelListener;

import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import java.util.Vector;

public class DOMTreeModel implements TreeModel, DOMTreeNotifier {
    private Node rootNode;
    private Vector treeModelListeners = new Vector();

    public DOMTreeModel(Node node) {
	rootNode = node;
    }
    public void addTreeModelListener(TreeModelListener l) {
	// use addElement instead of add for jdk1.1.x compatibility.
	treeModelListeners.addElement(l);	
    }
    public void removeTreeModelListener(TreeModelListener l) {
	treeModelListeners.removeElement(l);
    }
    public Object getChild(Object parent, int index) {
	return ((Node)parent).getChildNodes().item(index);
    }
    public int	getChildCount(Object parent) {
	return ((Node)parent).getChildNodes().getLength();
    }
    public int getIndexOfChild(Object parent, Object child) {
	NodeList childNodes = ((Node)parent).getChildNodes();
	int res = -1;
	int length = childNodes.getLength();
	for (int i = 0; i < length; i++) {
	    if (childNodes.item(i) == child) {
		res = i;
		break;
	    }
	}
	return res;
	
    }
    public Object getRoot() {
	return rootNode;
    }
    public boolean isLeaf(Object node) {
	return getChildCount(node) == 0;
    }

    public void valueForPathChanged(TreePath path, Object newValue) {
	return;
    }


    /*
     * Invoked after a node (or a set of siblings) has changed in some way.
     */
    public void treeNodesChanged(TreeModelEvent e)  {
    	for (int i = 0; i < treeModelListeners.size() ; i++) {
            ((TreeModelListener)treeModelListeners.elementAt(i)).
		treeNodesChanged(e);
        }
    }
                
    /*
     * Invoked after nodes have been inserted into the tree.
     */
    public void treeNodesInserted(TreeModelEvent e) {
    	for (int i = 0; i < treeModelListeners.size(); i++) {
            ((TreeModelListener)treeModelListeners.elementAt(i)).
		treeNodesInserted(e);
        }
    }
    
    /*
     * Invoked after nodes have been removed from the tree.
     */
    public void treeNodesRemoved(TreeModelEvent e)  {
    	for (int i = 0; i < treeModelListeners.size(); i++) {
            ((TreeModelListener)treeModelListeners.elementAt(i)).
		treeNodesRemoved(e);
        }
    }
    /*
     * Invoked after the tree has drastically changed structure from a given node down.
     */
    public void treeStructureChanged(TreeModelEvent e) {
    	for (int i = 0; i < treeModelListeners.size(); i++) {
            ((TreeModelListener)treeModelListeners.elementAt(i)).
		treeStructureChanged(e);
        }
    }
}
