/*
 * $Id: BookmarksTest.java,v 1.1 2003/09/28 06:29:18 edburns%acm.org Exp $
 */

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun
 * Microsystems, Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): Ed Burns &lt;edburns@acm.org&gt;
 */

package org.mozilla.webclient;

import junit.framework.TestSuite;
import junit.framework.Test;

import java.util.Enumeration;
import javax.swing.tree.TreeModel;
import javax.swing.tree.TreeNode;

// BookmarksTest.java

public class BookmarksTest extends WebclientTestCase {

    public BookmarksTest(String name) {
	super(name);
    }

    public static Test suite() {
	return (new TestSuite(BookmarksTest.class));
    }

    //
    // Constants
    // 

    //
    // Testcases
    // 

    public void testBookmarks() throws Exception {
	BrowserControl firstBrowserControl = null;
	BrowserControlFactory.setAppData(getBrowserBinDir());
	firstBrowserControl = BrowserControlFactory.newBrowserControl();
	assertNotNull(firstBrowserControl);

	// test we can get the bookmarks
	TreeModel tree = null;
	TreePrinter printer = new TreePrinter();
	Bookmarks bookmarks = (Bookmarks)
	    firstBrowserControl.queryInterface(BrowserControl.BOOKMARKS_NAME);
	assertNotNull(bookmarks);

	// test that we can get the bookmarks tree
	tree = bookmarks.getBookmarks();
	assertNotNull(bookmarks);

	walkTree((TreeNode) tree.getRoot(), printer, null);

	BrowserControlFactory.deleteBrowserControl(firstBrowserControl);
	BrowserControlFactory.appTerminate();
    }

    interface NodeCallback {
	public void takeActionOnNode(TreeNode node, int depth, Object closure);
    }

    class TreePrinter extends Object implements NodeCallback {
	public void takeActionOnNode(TreeNode node, int depth, Object closure){
	    for (int i = 0; i < depth; i++) {
		System.out.print("  ");
	    }
	    System.out.println(node.toString());
	}
    }

    protected int depth = 0;

    protected void walkTree(TreeNode root, NodeCallback cb, Object closure) {
	
	Enumeration children = root.children();
	TreeNode child = null;

	cb.takeActionOnNode(root, depth, closure);
	while (children.hasMoreElements()) {
	    depth++;
	    walkTree((TreeNode) children.nextElement(), cb, closure);
	    depth--;
	}
    }

}
