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
 */

import org.mozilla.pluglet.*;
import org.mozilla.pluglet.mozilla.*;

import org.mozilla.dom.*;

import java.awt.print.*;
import java.awt.*;

import javax.swing.tree.*;
import javax.swing.event.*;
import javax.swing.*;
import java.util.*;

import org.w3c.dom.*;

public class DOMViewerFactory implements PlugletFactory {
    public DOMViewerFactory() {
    }

    public Pluglet createPluglet(String mimeType) {
 	return new DOMViewer();
    }

    public void initialize(PlugletManager manager) {	
    }

    public void shutdown() {
    }

}

interface DOMTreeNotifier {
    /*
     * Invoked after a node (or a set of siblings) has changed in some way.
     */
    public void treeNodesChanged(TreeModelEvent e);
                
    /*
     * Invoked after nodes have been inserted into the tree.
     */
    public void treeNodesInserted(TreeModelEvent e);
    
    /*
     * Invoked after nodes have been removed from the tree.
     */
    public void treeNodesRemoved(TreeModelEvent e);
    /*
     * Invoked after the tree has drastically changed structure from a given node down.
     */
    public void treeStructureChanged(TreeModelEvent e);
};

class DOMTreeModel implements TreeModel, DOMTreeNotifier {
    private Node rootNode;
    private Vector treeModelListeners = new Vector();
    public DOMTreeModel(Node node) {
	rootNode = node;
    }
    public void addTreeModelListener(TreeModelListener l) {
	treeModelListeners.add(l);	
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
};

class DOMCellRenderer implements TreeCellRenderer {
    private TreeCellRenderer cellRenderer;
    public DOMCellRenderer(TreeCellRenderer cellRenderer) {
	this.cellRenderer = cellRenderer;
    }
    	
    public Component getTreeCellRendererComponent(JTree tree, Object value, 
						  boolean selected,
						  boolean expanded, 
						  boolean leaf, int row,
						  boolean hasFocus) {
	return cellRenderer.getTreeCellRendererComponent(tree, 
							 ((Node)value).getNodeName(),
							 selected,
							 expanded, 
							 leaf,  row, 
							 hasFocus);
    }

}

class DOMViewer implements Pluglet {
    private PlugletPeer peer;
    private Node rootNode;
    private JFrame frame;
    private JScrollPane treePane;
    private JPanel panel;
    private JTree tree;
    private DOMAccessPanel elementPanel;

    public DOMViewer() {

    }
    
    public void initialize(PlugletPeer peer) {
	try {
	this.peer = peer;
	PlugletTagInfo info = peer.getTagInfo();
	if (info instanceof PlugletTagInfo2) {
	    PlugletTagInfo2 info2 = (PlugletTagInfo2)info;
	    Element e = (Element) info2.getDOMElement();
	    Document doc = e.getOwnerDocument();
	    e = doc.getDocumentElement();
	    e.normalize();
	    rootNode = e;
	}
	DOMTreeModel treeModel = new DOMTreeModel(rootNode);
	elementPanel = new DOMAccessPanel(treeModel);
	tree = new JTree(treeModel );
	tree.addTreeSelectionListener(elementPanel);
	tree.setCellRenderer(new DOMCellRenderer(tree.getCellRenderer()));
	tree.getSelectionModel().setSelectionMode(TreeSelectionModel.SINGLE_TREE_SELECTION);
	treePane = new JScrollPane(tree);
	panel = new JPanel();
	panel.setLayout(new BorderLayout());
	panel.add("Center", treePane);
	} catch (Exception e) {
	    e.printStackTrace();
	}
    }

    public void start() {
    }

    public void stop() {
    }

    public void destroy() {
    }

    public PlugletStreamListener newStream() {
	return null;
    }

    public void setWindow(Frame fr) {
	if(fr == null) {
	    if (frame != null) {
		frame.hide();
		frame = null;
	    }
	    return;
	}
	if (frame == null) {
	    frame  = new JFrame("DOM Viewer");
	    frame.getContentPane().setLayout(new BorderLayout());
	    frame.getContentPane().add(new JSplitPane(JSplitPane.VERTICAL_SPLIT, panel, elementPanel));
	    frame.pack();
	    frame.show();
	}
    }

    public void print(PrinterJob printerJob) {
    }

}
