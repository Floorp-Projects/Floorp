package org.mozilla.webclient.test;

import javax.swing.JTree;
import javax.swing.tree.TreeModel;
import javax.swing.event.TreeSelectionListener;
import javax.swing.event.TreeSelectionEvent;
import javax.swing.tree.TreeSelectionModel;
import java.net.URL;
import java.io.IOException;
import javax.swing.JEditorPane;
import javax.swing.JScrollPane;
import javax.swing.JSplitPane;
import javax.swing.JFrame;
import java.awt.*;
import java.awt.event.*;

public class BookmarksFrame extends JFrame
{

public JTree tree;

public BookmarksFrame(TreeModel yourTree)
{
    super("bookmarks");
    tree = new JTree(yourTree);
    this.getContentPane().add(tree, BorderLayout.CENTER);
}

}
