/* -*- Mode: java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s): Kirk Baker <kbaker@eb.com>
 *               Ian Wilkinson <iw@ennoble.com>
 *               Mark Goddard
 *               Ed Burns <edburns@acm.org>
 */

package org.mozilla.webclient.test;

import org.mozilla.util.Assert;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.w3c.dom.Element;
import org.w3c.dom.Document;

import org.w3c.dom.events.Event;
import org.w3c.dom.events.EventTarget;
import org.w3c.dom.events.EventListener;
import org.w3c.dom.events.MouseEvent;

import javax.swing.JScrollPane;
import javax.swing.JSplitPane;
import javax.swing.JPanel;
import javax.swing.JFrame;
import javax.swing.JTree;

import javax.swing.tree.TreeSelectionModel;
import javax.swing.tree.TreePath;

import java.awt.BorderLayout;

import java.util.Stack;

/**
 *

 * A dom viewer Frame

 *
 * @version $Id: DOMViewerFrame.java,v 1.3 2000/06/30 17:53:58 edburns%acm.org Exp $
 * 
 * @see	org.mozilla.webclient.BrowserControlFactory

 * Based on DOMViewerFactor.java by idk

 * Contributor(s): Ed Burns <edburns@acm.org>
 * Igor Kushnirskiy <idk@eng.sun.com>

 */





public class DOMViewerFrame extends JFrame implements EventListener {

private EmbeddedMozilla creator;

private Node rootNode;
private Node mouseOverNode = null;
private DOMAccessPanel elementPanel;

private JScrollPane treePane;
private JSplitPane splitPane;
private JPanel panel;
private JTree tree;

private Document doc;

private Stack pathStack;

    
public DOMViewerFrame (String title, EmbeddedMozilla Creator) 
{
    super(title);
    creator = Creator;

    this.getContentPane().setLayout(new BorderLayout());    

    // create the content for the top of the splitPane
    treePane = new JScrollPane();
    panel = new JPanel();
    panel.setLayout(new BorderLayout());
    panel.add("Center", treePane);

    // create the content for the bottom of the splitPane
    elementPanel = new DOMAccessPanel();
    
    // create the splitPane, adding the top and bottom content
    splitPane = new JSplitPane(JSplitPane.VERTICAL_SPLIT, panel, elementPanel);

    // add the splitPane to myself
    this.getContentPane().add(splitPane);
    this.pack();
    splitPane.setDividerLocation(0.35);

} // DOMViewerFrame() ctor

public void setDocument(Document newDocument)
{
    if (null == newDocument) {
        return;
    }
    EventTarget eventTarget = null;

    if (null != doc) {
        if (doc instanceof EventTarget) {
            eventTarget = (EventTarget) doc;
            eventTarget.removeEventListener("mousedown", this, false);
        }
    }
    doc = newDocument;
    if (doc instanceof EventTarget) {
        eventTarget = (EventTarget) doc;
        eventTarget.addEventListener("click", this, false);
        eventTarget.addEventListener("mouseover", this, false);
    }

    try {
	// store the document as the root node
	Element e;
	e = doc.getDocumentElement();
	e.normalize();
	rootNode = e;
	
	// create a tree model for the DOM tree
	DOMTreeModel treeModel = new DOMTreeModel(rootNode);

	// hook the treeModel up to the elementPanel
	elementPanel.setTreeNotifier(treeModel);
	tree = new JTree(treeModel);

	// hook the elementPanel up to the treeModel
	tree.addTreeSelectionListener(elementPanel);

	tree.setCellRenderer(new DOMCellRenderer(tree.getCellRenderer()));
	tree.getSelectionModel().setSelectionMode(TreeSelectionModel.DISCONTIGUOUS_TREE_SELECTION);
	treePane.setViewportView(tree);
    } catch (Exception e) {
	e.printStackTrace();
    }
    
}

// 
// From org.w3c.dom.events.EventListener
// 

public void handleEvent(Event e)
{
    if (!(e instanceof MouseEvent)) {
        return;
    }
    MouseEvent mouseEvent = (MouseEvent) e;
    Node relatedNode = (Node) mouseEvent.getRelatedTarget();
    if (null != relatedNode) {
        mouseOverNode = relatedNode;
    }
    if (mouseEvent.getShiftKey() && 1 == mouseEvent.getButton()) {
        if (null != mouseOverNode) {
            selectNodeInTree(mouseOverNode);
        }
    }
}

protected void selectNodeInTree(Node node)
{
    if (null == node) {
        return;
    }

    if (null != pathStack) {
        pathStack.clear();
    }
    populatePathStackFromNode(node);
    if (null == pathStack || pathStack.isEmpty()) {
        return;
    }

    // don't include the root
    int i, j = 0, size = pathStack.size() - 1;
    
    // reverse the stack
    Object pathArray[] = new Object [size];
    for (i=size; i > 0; i--) {
        pathArray[j++] = pathStack.elementAt(i - 1);
    }
    
    TreePath nodePath = new TreePath(pathArray);
    
    tree.clearSelection();
    tree.setSelectionPath(nodePath);
}

protected void populatePathStackFromNode(Node node)
{
    if (null == node) {
        return;
    }
    if (null == pathStack) {
        // PENDING(edburns): perhaps provide default size
        pathStack = new Stack();
    }
    if (null == pathStack) {
        return;
    }
    pathStack.push(node);
    Node parent = node.getParentNode();
    if (null != parent) {
        populatePathStackFromNode(parent);
    }
}


}

// EOF
