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
 * Contributor(s): Denis Sharypov <sdv@sparc.spb.su>
 *                 Igor Kushnirskiy <idk@eng.sun.com>
 */

package org.mozilla.webclient.test;

import java.awt.*;
import java.awt.event.*;
import org.mozilla.dom.util.DOMTreeNotifier;
import org.w3c.dom.*;
import org.mozilla.dom.*;
import javax.swing.*;
import javax.swing.tree.*; //idk
import javax.swing.border.*;
import javax.swing.event.*;
import javax.swing.JFileChooser;

public class DOMAccessPanel extends JPanel implements ActionListener, ItemListener,  TreeSelectionListener { 

    private JTextField name, aValue;
    private JComboBox type, aName;
    private JTextArea value;
    private JButton newNode, insert, append, remove, set, removeAttr, save;
    private Node node, prv;
    private NamedNodeMap attrMap;
    private boolean updating;
    private String[] types = {
	"ELEMENT",
	"ATTRIBUTE",
	"TEXT",
	"CDATA_SECTION",
	"ENTITY_REF",
	"ENTITY",
	"PROC_INSTR",
	"COMMENT",
	"DOCUMENT",
	"DOCUMENT_TYPE",
	"DOC_FRAGM",
	"NOTATION"
    };	

//      private boolean debug = true;
    private boolean debug = false;

    private Component tree;
    private DOMTreeNotifier treeNotifier;
    private TreePath nodePath;
    private DOMTreeDumper treeDumper;
    private JFileChooser chooser;


    public DOMAccessPanel() {

	GridBagLayout layout = new GridBagLayout();
	GridBagConstraints c = new GridBagConstraints();
	JPanel nodeInfo = new JPanel();
	nodeInfo.setLayout(layout);


	JLabel l = new JLabel("Name:");
	c.anchor = GridBagConstraints.WEST;
	c.fill = GridBagConstraints.BOTH;
	layout.setConstraints(l, c);
	nodeInfo.add(l);

	l = new JLabel("Type:");
	c.gridwidth = GridBagConstraints.REMAINDER; //end row
	layout.setConstraints(l, c);
	nodeInfo.add(l);

	c.gridwidth = GridBagConstraints.RELATIVE; //new  row
	name = new JTextField(10);
	layout.setConstraints(name, c);
	nodeInfo.add(name);

	c.gridwidth = GridBagConstraints.REMAINDER; //end row
	type = new JComboBox(types);
	layout.setConstraints(type, c);
	nodeInfo.add(type);	

	// Attribute panel

	JPanel attrPanel = new JPanel();
	GridBagLayout attrLayout = new GridBagLayout();
	attrPanel.setLayout(attrLayout);
	
	c.gridwidth = GridBagConstraints.RELATIVE; //new  row
	aName = new JComboBox();
	aName.setEditable(true);
	aName.addItemListener(this);
	attrLayout.setConstraints(aName, c);
	attrPanel.add(aName);

	JPanel p = new JPanel();
	l = new JLabel("=");
	p.add(l);
	aValue = new JTextField(9);
	p.add(aValue);
	c.gridwidth = GridBagConstraints.REMAINDER; //end row
	attrLayout.setConstraints(p, c);
	attrPanel.add(p);

	p = new JPanel();
	set = new JButton("Set");
	set.addActionListener(this);
	p.add(set);
	removeAttr = new JButton("Remove");
	removeAttr.setActionCommand("remove_attr");
	removeAttr.addActionListener(this);
	p.add(removeAttr);
	attrLayout.setConstraints(p, c);
	attrPanel.add(p);
	attrPanel.setBorder(new TitledBorder(new BevelBorder(BevelBorder.LOWERED), " Attributes "));
  	layout.setConstraints(attrPanel, c);	

	// End Attribute panel

	nodeInfo.add(attrPanel);

	l = new JLabel("Value:");
	layout.setConstraints(l, c);
	nodeInfo.add(l);

	value = new JTextArea(5, 23);
	value.addCaretListener (new CaretListener() {
	    public void caretUpdate(CaretEvent e) {
		dbg("caret:");   
		if (node == null || updating) return;
		String valueStr = value.getText();
		if (!valueStr.equals("")) {
		    node.setNodeValue(valueStr);
		}
	    }
	});

	c.fill = GridBagConstraints.BOTH;
	JScrollPane valueScrollPane = new JScrollPane(value);
	layout.setConstraints(valueScrollPane, c);
	nodeInfo.add(valueScrollPane);

	JPanel nodeButtonPanel = new JPanel();
	GridLayout gl = new GridLayout(2, 2);
	nodeButtonPanel.setLayout(gl);
	newNode = new JButton("New");
	newNode.setToolTipText("Create a new node");
	newNode.addActionListener(this);
	nodeButtonPanel.add(newNode);
	insert = new JButton("Insert");
	insert.setToolTipText("Inserts a node before the selected node ");
	insert.addActionListener(this);
	nodeButtonPanel.add(insert);
	append = new JButton("Append");
	append.setToolTipText("Adds a node to the end of the list of children of the selected node");
	append.addActionListener(this);
	nodeButtonPanel.add(append);
	remove = new JButton("Remove");
	remove.setToolTipText("Removes the selected node from the tree");
	remove.addActionListener(this);
	nodeButtonPanel.add(remove);
	nodeButtonPanel.setBorder(new TitledBorder(new BevelBorder(BevelBorder.LOWERED), " Node Manipulation "));

	c.fill = GridBagConstraints.HORIZONTAL;
	layout.setConstraints(nodeButtonPanel, c);

	nodeInfo.add(nodeButtonPanel);
	nodeInfo.setBorder(new TitledBorder(new EtchedBorder(), " Node info "));

	
	layout = new GridBagLayout();
	setLayout(layout);
	layout.setConstraints(nodeInfo, c);
	add(nodeInfo);

	save = new JButton("Dump Tree To File");
	save.setActionCommand("save");
	save.addActionListener(this);
	c.anchor = GridBagConstraints.CENTER;
	c.fill = GridBagConstraints.HORIZONTAL;
	layout.setConstraints(save, c);
	add(save);
	setMinimumSize(new Dimension(350, 400));
	setButtonState();

    }

    public void setTreeNotifier(DOMTreeNotifier newTreeNotifier)
    {
	treeNotifier = newTreeNotifier;
    }

    public void actionPerformed(ActionEvent e) {
	String command = e.getActionCommand();
	dbg("AttrInfoPanel.actionPerformed: " + command);
	if (command.equalsIgnoreCase("new")) {
	    prv = node;
	    updateInfo(null);
	    type.setEnabled(true);
	    insert.setEnabled(true);
	    append.setEnabled(true);
	} else if (command.equalsIgnoreCase("insert")) {
	    insertNode();
	    treeNotifier.treeStructureChanged(
	        new TreeModelEvent(this, nodePath.getParentPath()));
	} else if (command.equalsIgnoreCase("append")) {
	    appendNode();
	    treeNotifier.treeStructureChanged(
	        new TreeModelEvent(this, nodePath.getParentPath()));
	} else if (command.equalsIgnoreCase("remove")) {
	    if (node == null) return;
	    Node parent = node.getParentNode();
	    parent.removeChild(node);
	    node = parent;
	    treeNotifier.treeStructureChanged(new TreeModelEvent(this, nodePath.getParentPath()));
	} else if (command.equalsIgnoreCase("save")) {
	    saveDoc();
	} else if (command.equalsIgnoreCase("set")) {
	    ((Element)node).setAttribute((String)aName.getSelectedItem(), aValue.getText());
	    updateInfo(node);
	} else if (command.equalsIgnoreCase("remove_attr")) {
	    removeAttribute();
	}
    }

    public void itemStateChanged(ItemEvent e) {
	if ((node != null) && (aName.getSelectedItem() != null)) {
	    set.setEnabled(true);
	    if (((attrMap = node.getAttributes()) != null) &&
	        (aName.getSelectedIndex() >=0)) {
		dbg("setting attr...");   
		aValue.setText(attrMap.item(aName.getSelectedIndex()).getNodeValue());
	    }
	}
    }

    public  void updateInfo(Node node) {
	updating = true;
	dbg("updateInfo" + node);
	this.node = node;
	setButtonState();
	name.setText("");
	value.setText("");
	aValue.setText("");
	aName.removeAllItems();
	if (node == null) {
	    updating = false;
	    return;
	}
	prv = node;
	name.setText(node.getNodeName());
	type.setSelectedIndex(node.getNodeType() - 1);
	dbg("1 update node name: " + node.getNodeName());
	dbg("1 update node value:" + node.getNodeValue());
	value.setText(node.getNodeValue());
	dbg("2 update node name: " + node.getNodeName());
	dbg("2 update node value:" + node.getNodeValue());
	attrMap = node.getAttributes();
	if (attrMap == null) {
	    updating = false;
	    return;
	}
	int length = attrMap.getLength();
	for (int i=0; i < length; i++) {
	    Attr a = (Attr)attrMap.item(i);
	    aName.addItem(a.getName());
	}
	if (length > 0) {
	    aName.setSelectedIndex(0);
	    aValue.setText(attrMap.item(0).getNodeValue());
	}
	updating = false;
    }

    public void  valueChanged(TreeSelectionEvent e) {
	dbg("DOMAccessPanel.valueChanged");
	if (e.isAddedPath()) {
	    updateInfo((Node)e.getPath().getLastPathComponent());
	}
	nodePath = e.getPath();
	tree = (Component)e.getSource();
    }

    private Node createNode() {
	int nodeType = type.getSelectedIndex();
	Document doc = prv.getOwnerDocument();
	Node newNode = null;
	switch (nodeType + 1) {
	case Node.ELEMENT_NODE: 
	    dbg("ELEMENT"); 
	    newNode = doc.createElement(name.getText());
	    break;
	case Node.ATTRIBUTE_NODE: 
	    dbg("ATTRIBUTE"); 
	    return null;
	case Node.TEXT_NODE: 
	    dbg("TEXT"); 
	    newNode = doc.createTextNode(value.getText());
	    break;
	case Node.CDATA_SECTION_NODE: 
	    dbg("CDATA_SECTION"); 
	    return null;
	case Node.ENTITY_REFERENCE_NODE: 
	    dbg("ENTITY_REFERENCE"); 
	    return null;
	case Node.ENTITY_NODE: 
	    dbg("ENTITY"); 
	    return null;
	case Node.PROCESSING_INSTRUCTION_NODE: 
	    dbg("PROCESSING_INSTRUCTION"); 
	    return null;
	case Node.COMMENT_NODE: 
	    dbg("COMMENT"); 
	    newNode = doc.createComment(value.getText());
	    break;
	case Node.DOCUMENT_NODE: 
	    dbg("DOCUMENT"); 
	    return null;
	case Node.DOCUMENT_TYPE_NODE: 
	    dbg("DOCUMENT_TYPE"); 
	    return null;
	case Node.DOCUMENT_FRAGMENT_NODE: 
	    dbg("DOCUMENT_FRAGMENT"); 
	    return null;
	case Node.NOTATION_NODE: 
	    dbg("NOTATION"); 
	    return null;
	}
	return newNode;
    }

    private void insertNode() {
	Node newNode = createNode();
	if (newNode == null) return;
	dbg("inserting...");
  	prv.getParentNode().insertBefore(newNode, prv);
    }

    private void appendNode() {
	Node newNode = createNode();
	if (newNode == null) return;
	dbg("appending...");
  	prv.appendChild(newNode);
    }

    private void removeAttribute() {
	    ((Element)node).removeAttribute((String)aName.getSelectedItem());
	    updateInfo(node);
    }

    private void saveDoc() {
	if (chooser == null) {
	    chooser = new JFileChooser();
	}
	int returnVal = chooser.showSaveDialog(this);
	if(returnVal == JFileChooser.APPROVE_OPTION) {
	    String fileName = chooser.getSelectedFile().getPath();
	    if (treeDumper == null) {
		treeDumper = new DOMTreeDumper(debug);	
	    }
	    treeDumper.dumpToFile(fileName, node.getOwnerDocument());
	} 	
    }

    private void setButtonState() {
	boolean nodeExists = (node != null);
	boolean attrExists = nodeExists && !(node.getAttributes() == null || node.getAttributes().getLength() == 0);
	type.setEnabled(nodeExists);
	newNode.setEnabled(nodeExists);
	insert.setEnabled(nodeExists);
	append.setEnabled(nodeExists);
	save.setEnabled(nodeExists);
	remove.setEnabled(nodeExists);
	set.setEnabled(attrExists);
	removeAttr.setEnabled(attrExists);
    }

    private void dbg(String str) {
	if (debug) {
	    System.out.println(str);
	}
    }
}
