/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 *  except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/

 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is MozillaTranslator (Mozilla Localization Tool)
 *
 * The Initial Developer of the Original Code is Henrik Lynggaard Hansen
 *
 * Portions created by Henrik Lynggard Hansen are
 * Copyright (C) Henrik Lynggaard Hansen.
 * All Rights Reserved.
 *
 * Contributor(s):
 * Henrik Lynggaard Hansen (Initial Code)
 *
 */
package org.mozilla.translator.gui;

import java.awt.BorderLayout;

import java.util.*;
import javax.swing.*;
import javax.swing.event.*;
import javax.swing.table.*;
import javax.swing.tree.*;

import org.mozilla.translator.datamodel.*;
import org.mozilla.translator.kernel.*;
import org.mozilla.translator.gui.models.*;

/**
 *
 * @author  Henrik
 * @version 4.0
 */
public class ChromeView extends JInternalFrame implements TreeSelectionListener,MozFrame
{

    private List cols;
    private String localeName;
    private ComplexTableModel model;
    private JTree tree;
    private JTable table;
    private DefaultMutableTreeNode root;
    private JScrollPane treeScroll;
    private JScrollPane tableScroll;
    private JSplitPane split;


    /** Creates new ChromeView */
    public ChromeView(List wCols,String l)
    {
        super("Chrome view for " + l);

        MozInstall notLoadedInstall;
        MozComponent notLoadedComponent;
        MozComponent notLoadedSubcomponent;
        MozFile notLoadedFile;
        Phrase notLoadedPhrase;
        List notLoadedList;

        cols = wCols;
        localeName=l;

        setClosable(true);
        setMaximizable(true);
        setIconifiable(true);
        setResizable(true);

        notLoadedInstall      = new MozInstall("not loaded","not loaded");
        notLoadedComponent    = new MozComponent("not loaded",notLoadedInstall);
        notLoadedSubcomponent = new MozComponent("not loaded",notLoadedComponent);
        notLoadedFile         = new MozFile("not loaded",notLoadedSubcomponent);
        notLoadedPhrase       = new Phrase("not loaded",notLoadedFile,"not loaded","not loaded",false);
        notLoadedList = new ArrayList();
        notLoadedList.add(notLoadedPhrase);

        root = Glossary.getDefaultInstance().getTreeRoot();

        tree = new JTree(root);
        tree.addTreeSelectionListener(this);
        tree.setRootVisible(false);
        treeScroll = new JScrollPane(tree);


        model = new ComplexTableModel(notLoadedList,cols,localeName);
        table = new JTable(model);
        tableScroll = new JScrollPane(table);

        split = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT,treeScroll,tableScroll);

        getContentPane().add(split,BorderLayout.CENTER);
        pack();
        MainWindow.getDefaultInstance().addWindow(this);


        split.setOneTouchExpandable(true);
        try
        {
            setMaximum(true);
        }
    catch (Exception e) { /* silently ignore */ }
        split.setDividerLocation(0.30);
        setVisible(true);
    }

    public void valueChanged(TreeSelectionEvent evt)
    {
        TreePath tp;
        int pathCount;
        List relevant;
        DefaultMutableTreeNode lastNode;
        MozFile theFile;
        List ps;
        tp = evt.getNewLeadSelectionPath();

        pathCount= tp.getPathCount();

        if (pathCount==5)
        {
            lastNode = (DefaultMutableTreeNode) tp.getLastPathComponent();

            theFile = (MozFile) lastNode.getUserObject();
            ps = theFile.getAllChildren();

            model = new ComplexTableModel(ps,cols,localeName);

            table.setModel(model);
        }
    }






    public Phrase getSelectedPhrase()
    {
        int rowIndex;
        rowIndex = table.getSelectedRow();
        return model.getRow(rowIndex);

    }
    public String getLocaleName()
    {
        return localeName;
    }

    public JTable getTable() 
    {
        return table;
    }
}