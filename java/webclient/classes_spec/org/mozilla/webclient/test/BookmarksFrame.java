package org.mozilla.webclient.test;

import javax.swing.JTree;
import javax.swing.tree.TreeModel;
import javax.swing.event.TreeSelectionListener;
import javax.swing.event.TreeSelectionEvent;
import javax.swing.tree.TreeSelectionModel;
import javax.swing.tree.TreePath;
import java.net.URL;
import java.io.IOException;
import javax.swing.JEditorPane;
import javax.swing.JScrollPane;
import javax.swing.JSplitPane;
import javax.swing.JFrame;
import java.awt.*;
import java.awt.event.*;

import org.mozilla.webclient.Navigation;
import org.mozilla.webclient.BrowserControl;
import org.mozilla.webclient.BookmarkEntry;

public class BookmarksFrame extends JFrame implements TreeSelectionListener, MouseListener
{

private JTree tree;
private JScrollPane scrollPane;
private BrowserControl bc;
private Navigation navigation;
private TreePath currentPath;

public BookmarksFrame(TreeModel yourTree, BrowserControl yourBc)
{
    super("bookmarks");
    this.getContentPane().setLayout(new BorderLayout());    
    
    tree = new JTree(yourTree);
    scrollPane = new JScrollPane(tree);
    bc = yourBc;
    try {
	navigation = 
	    (Navigation)bc.queryInterface(BrowserControl.NAVIGATION_NAME);
    }
    catch (Exception e) {
	System.out.println("BookmarksFrame Can't get Navigation from BrowserControl");
    }
    this.getContentPane().add(scrollPane, BorderLayout.CENTER);
    tree.addTreeSelectionListener(this);
    tree.addMouseListener(this);
}

//
// From TreeSelectionListener
//

public void valueChanged(TreeSelectionEvent e)
{
    currentPath = e.getPath();
    if (null == currentPath) {
	return;
    }

    System.out.println("debug: edburns: TreePath: " + currentPath.toString());
}

//
// From MouseListener
// 

public void mouseClicked(java.awt.event.MouseEvent e)
{
    if (null == currentPath || e.getClickCount() < 2) {
	return;
    }
    
    Object lastComponent = currentPath.getLastPathComponent();
    if (!(lastComponent instanceof BookmarkEntry)) {
	return;
    }
    BookmarkEntry currentBookmark = (BookmarkEntry) lastComponent;
    if (!currentBookmark.isFolder()) {
	navigation.loadURL(currentBookmark.toString());
    }
}

public void mouseEntered(java.awt.event.MouseEvent e)
{
}

public void mouseExited(java.awt.event.MouseEvent e)
{
}

public void mousePressed(java.awt.event.MouseEvent e)
{
}

public void mouseReleased(java.awt.event.MouseEvent e)
{
}

}
