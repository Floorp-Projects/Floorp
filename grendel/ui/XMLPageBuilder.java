/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1997
 * Netscape Communications Corporation.  All Rights Reserved.
 *
 * Created: Giao Nguyen <grail@cafebabe.org>, 13 Jan 1999.
 */

package grendel.ui;

import java.io.InputStream;
import java.io.IOException;

import java.net.URL;
import java.util.Hashtable;
import java.util.Properties;
import java.util.StringTokenizer;
import java.util.NoSuchElementException;

import java.awt.Container;
import java.awt.GridBagLayout;
import java.awt.GridBagConstraints;
import java.awt.Insets;

import javax.swing.JComponent;
import javax.swing.JFrame;
import javax.swing.JDialog;
import javax.swing.ButtonGroup;
import javax.swing.JPanel;
import javax.swing.JButton;
import javax.swing.RootPaneContainer;
import javax.swing.JRadioButton;
import javax.swing.JTextField;
import javax.swing.JLabel;
import javax.swing.SwingConstants;

import org.w3c.dom.Element;
import org.w3c.dom.Node;

import com.sun.xml.parser.Resolver;
import com.sun.xml.parser.Parser;
import com.sun.xml.tree.XmlDocument;
import com.sun.xml.tree.XmlDocumentBuilder;
import com.sun.xml.tree.TreeWalker;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;
import org.xml.sax.SAXParseException;



/**
 * Build a panel from an XML data source.
 */
public class XMLPageBuilder extends XMLWidgetBuilder {
  static final String id_attr = "id";
  static final String label_attr = "label";
  static final String separator_attr = "separator";
  static final String radio_attr = "radio";
  static final String group_attr = "group";
  static final String accel_attr = "accel";
  static final String type_attr = "type";
  static final String dialog_tag = "dialog";
  static final String panel_tag = "panel";
  static final String input_tag = "input";
  static final String layout_attr = "layout";
  static final int ELEMENT_TYPE = 1;

  JPanel component;
  String title;
  String id;
  String attr;

  Hashtable input = new Hashtable();

  /**
   * Build a menu builder which operates on XML formatted data
   * 
   * @param attr attribute
   * @param id the value of the attribute to have a match
   */
  public XMLPageBuilder(String attr, String id) {
    this.attr = attr;
    this.id = id;
  }

  /**
   * Read the input stream and build a menubar from it
   * 
   * @param stream the stream containing the XML data
   */
  public void buildFrom(InputStream stream) {
    XmlDocument doc;
    TreeWalker tree;
    Node node;
    URL linkURL;
    Element current;

    try {
      doc = XmlDocumentBuilder.createXmlDocument(stream, false);
      current = doc.getDocumentElement();
      tree = new TreeWalker(current);

      node = 
        tree.getNextElement("head").getElementsByTagName("link").item(0);

      // set the configuration contained in this node
      setConfiguration((Element)node);

      // skip to the body
      buildFrom(new TreeWalker(tree.getNextElement("body")));
    } catch (Throwable t) {
      t.printStackTrace();
    }
  }

  /**
   * Set the element as the item containing configuration for the 
   * builder
   *
   * @param config the element containing configuration data
   */
  public void setConfiguration(Element config) {
    try {
      URL linkURL;
      // get the string properties
      if (config.getAttribute("href") != null 
	  && config.getAttribute("role").equals("stringprops")
	  && config.getTagName().equals("link")) {
	linkURL = getClass().getResource(config.getAttribute("href"));
	properties = new Properties();
	if (linkURL != null) properties.load(linkURL.openStream());
      }
    } catch (IOException io) {
      io.printStackTrace();
    }
  }

  /**
   * Build a menu bar from the data in the tree
   *
   * @param tree the tree containing the full set of a menubar tag
   */
  public void buildFrom(TreeWalker tree) {
    Element current  ;
    Node node;

    node = tree.getCurrent();
    current = (Element)node;
    
    // iterate through every node
    node = node.getFirstChild();
    node = node.getNextSibling();
    
    // at the very first menu tag
    while (node != null) {
      if (conditionMatch(node)) {
	processNode(node, component);
	break;
      }
      node = node.getNextSibling();
    }
  }

  boolean conditionMatch(Node node) {
    boolean match = false;
    if (node.getNodeType() == ELEMENT_TYPE) {
      Element current = (Element)node;
      String id_str = current.getAttribute(attr);

      // is the id the same as the on we're looking for?
      match = (id_str != null && id_str.equals(id)); 
    }

   return match;
  }

  /**
   * Process the node, extracting a widget and adding it as necessary.
   *
   * @param node the node to be processed
   * @param parent widget to parent node's extracted widget
   */
  protected void processNode(Node node, JComponent parent) {
    JComponent item = null;
    Element current;

    if (node == null) return;

    if (node.getNodeType() != ELEMENT_TYPE) {
      processNode(node.getNextSibling(), parent);
      return;
    }

    current = (Element)node;
    String tag = current.getTagName();

    if (tag.equals(dialog_tag)) { // dialog tag .. this gets us title
      title = getReferencedLabel(current, "title");
      node = node.getFirstChild().getNextSibling();
      processNode(node, component);
    } else if (tag.equals(panel_tag)) { // panel tag ... meat!
      JPanel panel = new JPanel();
      // first panel
      if (component == null) {
	component = panel;
      }
      panel.setLayout(new GridBagLayout());
      node = node.getFirstChild().getNextSibling();
      processNode(node, panel);
    } else {
      item = buildComponent(current, parent);

      // move on to the next item
      processNode(node.getNextSibling(), parent);
    }
  }

  /**
   * Build a constraint from the element's details.
   *
   * @param current the XML element containing constraint information.
   * @return extracted constraints
   */
  protected GridBagConstraints buildConstraints(Element current) {
    GridBagConstraints constraints = new GridBagConstraints();
    CustomInsets inset = 
      new CustomInsets(current.getAttribute("insets"));
    float weightx = 0.0f;
    String fill = null;
    String width = null;

    try {
      String s = current.getAttribute("weightx");
      if (s != null) {
	weightx = Float.valueOf(s).floatValue();
      }
    } catch (NumberFormatException nfe) {}

    // fill
    fill = current.getAttribute("fill");
    if (fill != null) {
      if (fill.trim().equals("horizontal")) {
	constraints.fill = GridBagConstraints.HORIZONTAL;
      }
    }

    // gridwidth
    width = current.getAttribute("gridwidth");
    if (width != null) {
      if (width.trim().equals("remainder")) {
	constraints.gridwidth = GridBagConstraints.REMAINDER;
      }
    }

    constraints.insets = inset;

    return constraints;
  }

  protected JComponent buildComponent(Element current) {
    JComponent item = null;
    String tag = current.getTagName();

    if (tag.equals("input")) { // input tag
      item = buildInput(current);
    } else if (tag.equals("label")) {
      item = buildLabel(current);
    }

    return item;
  }

  protected JTextField buildTextField(Element current) {
    JTextField item = null;
    if (current.getAttribute("columns") != null) {
      String s = current.getAttribute("columns");
      s = s.trim();
      try {
	int column = Integer.parseInt(s);
	item = new JTextField(column);
      } catch (NumberFormatException nfe) {
	item = new JTextField();
      }
    } else {
      item = new JTextField();
    }

    return item;
  }

  protected JRadioButton buildRadioButton(Element current) {
    JRadioButton item = null;
    String label = getReferencedLabel(current, "title");

    item = new JRadioButton();
    if (label != null) {
      item.setText(label);
    }
    return item;
  }

  protected JComponent buildInput(Element current) {
    JComponent item = null;
    String type = current.getAttribute("type");

    String ID = current.getAttribute("ID");
    if (type.equals("radio")) { // radio type
      item = buildRadioButton(current);
    } else if (type.equals("text")) { // text type
      item = buildTextField(current);
    } else if (type.equals("button")) { // buttons
      item = new JButton(getReferencedLabel(current, "command"));
    } else if (type.equals("custom")) {
      item = new JButton("Custom");
    }
    
    if (item != null && ID != null) {
      System.out.println("Adding " + ID + " to list");
      input.put(ID, item);
    }  

    return item;
  }
  
  protected JLabel buildLabel(Element current) {
    JLabel label = new JLabel(getReferencedLabel(current, "title"));
    label.setHorizontalAlignment(SwingConstants.RIGHT);
    return label;
  }

  public JPanel getComponent() {
    return component;
  }

  protected JComponent buildComponent(Element current, JComponent parent) {
    JComponent item = buildComponent(current);

    // item could be null if we don't know about the "type" tag for
    // example
    if (item != null) {
      GridBagConstraints cons = null;
      if (parent.getLayout() instanceof GridBagLayout) {
	cons = buildConstraints(current);
      }
      
      if (cons == null) {
	parent.add(item);
      } else {
	parent.add(item, cons);
      }
    } else {
      parent.add(item);
    }
    
    return item;
  }

  public String getTitle() {
    return title;
  }

  public static void main(String[] args) throws Exception {
    javax.swing.JFrame frame = new javax.swing.JFrame("Foo bar");
    XMLPageBuilder builder = new XMLPageBuilder(args[0], args[1]);
    URL url = builder.getClass().getResource("dialogs.xml");
    builder.buildFrom(url.openStream());
    JPanel panel = builder.getComponent();
    JDialog dialog = new JDialog();

    dialog.getContentPane().add(panel);
    dialog.setTitle(builder.getTitle());
    dialog.pack();
    dialog.setVisible(true);
  }

  class CustomInsets extends Insets {
    CustomInsets(String inset) {
      super(0, 0, 0, 0);
      
      inset = inset.trim();
      
      if (inset != null && inset.length() >= 9 
	  && (inset.charAt(0) == '[' 
	    || inset.charAt(inset.length() - 1) == ']')) {
	inset = inset.substring(1, inset.length() - 1);
	int[] val = parseInset(inset); // guaranteed to be a length of 4
	
	top = val[0];
      left = val[1];
      bottom = val[2];
      right = val[3];
      }
    }
    
    private int[] parseInset(String inset) {
      // if all else fails, it's 0
      int[] val = { 0, 0, 0, 0 };
      StringTokenizer tok = new StringTokenizer(inset, ",");
      
      // assign the values into
      for (int i = 0; i < val.length; i++) {
	try {
	  String s = tok.nextToken();
	  val[i] = Integer.parseInt(s);
	} catch (NoSuchElementException nse) {
	  // ignore. if it's fubared, we use 0 as the value
	} catch (NumberFormatException nfe) {
	  // ignore. if it's fubared, we use 0 as the value
	}
      }
      return val;
    }
  }
}

