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
 * The Initial Developer of the Original Code is Giao Nguyen
 * <grail@cafebabe.org>.  Portions created by Giao Nguyen are Copyright 
 * (C) 1999 Giao Nguyen.  All Rights Reserved.
 *
 * Contributors: Morgan Schweers <morgan@vixen.com>
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
import javax.swing.JList;
import javax.swing.JFrame;
import javax.swing.JDialog;
import javax.swing.ButtonGroup;
import javax.swing.JCheckBox;
import javax.swing.JPanel;
import javax.swing.JPasswordField;
import javax.swing.JButton;
import javax.swing.RootPaneContainer;
import javax.swing.JRadioButton;
import javax.swing.JTextField;
import javax.swing.JLabel;
import javax.swing.JScrollPane;
import javax.swing.SwingConstants;

import org.w3c.dom.Element;
import org.w3c.dom.Node;

import com.sun.xml.parser.Resolver;
import com.sun.xml.parser.Parser;
import com.sun.xml.tree.XmlDocument;
// import com.sun.xml.tree.XmlDocumentBuilder;
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

  PageUI component;
  String title;
  String id;
  String attr;
  PageModel model;
  Hashtable group = new Hashtable();

  /**
   * Build a menu builder which operates on XML formatted data
   * 
   * @param attr attribute
   * @param id the value of the attribute to have a match
   * @param model the page model for the page to be created
   * @param panel the PageUI to be used as the basis for this builder
   * @param reference the reference point used to find urls
   */
  public XMLPageBuilder(String attr, String id, PageModel model, 
                        PageUI panel, Class reference) {
    this(attr, id, model, panel);
    setReference(reference);
  }


  /**
   * Build a menu builder which operates on XML formatted data
   * 
   * @param attr attribute
   * @param id the value of the attribute to have a match
   * @param model the page model for the page to be created
   * @param panel the PageUI to be used as the basis for this builder
   */
  public XMLPageBuilder(String attr, String id, PageModel model, 
                        PageUI panel) {
    this(attr, id, model);
    component = panel;
  }

  /**
   * Build a menu builder which operates on XML formatted data
   * 
   * @param attr attribute
   * @param id the value of the attribute to have a match
   * @param model the page model for the page to be created
   */
  public XMLPageBuilder(String attr, String id, PageModel model) {
    this.attr = attr;
    this.id = id;
    this.model = model;
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
      doc = XmlDocument.createXmlDocument(stream, false);
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
    if (node.getNodeType() == Node.ELEMENT_NODE) {
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

    if (node.getNodeType() != Node.ELEMENT_NODE) {
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
      PageUI panel = new PageUI();
      // first panel
      if (component == null) {
	component = panel;
      }
      component.setLayout(new GridBagLayout());
      node = node.getFirstChild().getNextSibling();
      processNode(node, component);
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
    String height = null;

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

    width = current.getAttribute("gridwidth");
    if (width != null) {
      width = width.trim();
      try {
        constraints.gridwidth = Integer.parseInt(width);
      } catch (NumberFormatException nfe) {
        if (width.equals("remainder")) {
          constraints.gridwidth = GridBagConstraints.REMAINDER;
        } else if (width.equals("relative")) {
          constraints.gridwidth = GridBagConstraints.RELATIVE;
        }
      }
    }

    height = current.getAttribute("gridheight");
    if (height != null) {
      height = height.trim();
      try {
        constraints.gridheight = Integer.parseInt(height);
      } catch (NumberFormatException nfe) {
        if (width.equals("remainder")) {
          constraints.gridwidth = GridBagConstraints.REMAINDER;
        } else if (width.equals("relative")) {
          constraints.gridwidth = GridBagConstraints.RELATIVE;
        }
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

  protected JPasswordField buildPasswordField(Element current) {
    JPasswordField item = null;

    item = new JPasswordField();
    textFieldAdjustments(item, current);
    return item;
  }

  private void textFieldAdjustments(JTextField item, Element current) {
    if (current.getAttribute("columns") != null) {
      String s = current.getAttribute("columns");
      s = s.trim();
      try {
	int column = Integer.parseInt(s);
	item.setColumns(column);
      } catch (NumberFormatException nfe) {
      }
    }
  }

  protected JTextField buildTextField(Element current) {
    JTextField item = null;

    item = new JTextField();
    textFieldAdjustments(item, current);

    return item;
  }

  protected JRadioButton buildRadioButton(Element current) {
    JRadioButton item = null;
    String label = getReferencedLabel(current, "title");
    String group_str = current.getAttribute("group");

    // the label
    item = new JRadioButton();
    if (label != null) {
      item.setText(label);
    }

    // button group matters
    if (group_str != null) {
      ButtonGroup bg = null;
      if (group.containsKey(group_str)) {
        bg = (ButtonGroup)group.get(group_str);
      } else {
        bg = new ButtonGroup();
        group.put(group_str, bg);
      }
      bg.add(item);
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
    } else if (type.equals("password")) {
      item = buildPasswordField(current);
    } else if (type.equals("button")) { // buttons
      item = buildButton(current);
    } else if (type.equals("checkbox")) {
      item = buildCheckBox(current);
    } else if (type.equals("jlist")) {
      item = buildList(current);
    }
    
    return item;
  }

  protected JCheckBox buildCheckBox(Element current) {
    return new JCheckBox(getReferencedLabel(current, "title"));
  }

  protected JButton buildButton(Element current) {
    JButton button =  new JButton(getReferencedLabel(current, "title"));
    button.addActionListener(model);
    button.setActionCommand(getReferencedLabel(current, "command"));
    return button;
  }

  protected JList buildList(Element current) {
    JList list = new JList();

    return list;
  }
  
  protected JLabel buildLabel(Element current) {
    JLabel label = new JLabel(getReferencedLabel(current, "title"));
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
      
      if (cons == null) { // no constraints
        if (parent instanceof PageUI) {
          ((PageUI)parent).addCtrl(current.getAttribute("ID"), item);
        }
        else {
          parent.add(item);
        }
      } else { // we have constraints
        if (parent instanceof PageUI) {
          ((PageUI)parent).addCtrl(current.getAttribute("ID"), 
                                   item, cons);
        }
        else {
          parent.add(item, cons);
        }
      }
    }
    
    return item;
  }

  public String getTitle() {
    return title;
  }

  public static void main(String[] args) throws Exception {
    javax.swing.JFrame frame = new javax.swing.JFrame("Foo bar");
    PageModel model = new PageModel();
    XMLPageBuilder builder = new XMLPageBuilder(args[0], args[1], model);
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
