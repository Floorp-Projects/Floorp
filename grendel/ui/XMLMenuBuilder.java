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

import java.awt.Container;

import javax.swing.JComponent;
import javax.swing.JFrame;
import javax.swing.JMenuBar;
import javax.swing.JMenu;
import javax.swing.JMenuItem;
import javax.swing.JSeparator;
import javax.swing.JCheckBoxMenuItem;
import javax.swing.JRadioButtonMenuItem;
import javax.swing.ButtonGroup;

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

import grendel.widgets.MenuCtrl;
import grendel.widgets.MenuBarCtrl;

/**
 * Build a menu bar from an XML data source. This builder supports:
 * <UL>
 * <LI>Text label cross referencing to a properties file.
 * <LI>Action lookups.
 * </UL>
 */
public class XMLMenuBuilder extends XMLWidgetBuilder {
  static final String id_attr = "id";
  static final String menu_tag = "menu";
  static final String label_attr = "label";
  static final String separator_attr = "separator";
  static final String menuitem_tag = "menuitem";
  static final String checkbox_attr = "checkbox";
  static final String radio_attr = "radio";
  static final String group_attr = "group";
  static final String accel_attr = "accel";
  static final String type_attr = "type";

  /**
   * The button group indexed by its name.
   */
  protected Hashtable button_group;

  Hashtable actions;
  MenuBarCtrl component;

  /**
   * Build a menu builder which operates on XML formatted data
   * 
   * @param ref the reference point for properties location
   * @param actionList array of UIAction objects to map to
   */
  public XMLMenuBuilder(Class ref, UIAction[] actionList) {
    button_group = new Hashtable();
    actions = new Hashtable();
    this.ref = ref;

    if (actionList != null) {
      for (int i = 0; i < actionList.length; i++) {
	actions.put(actionList[i].getName(), actionList[i]);
      }
    }
  }


  /**
   * Build a menu builder which operates on XML formatted data
   * 
   * @param frame reference point for properties location
   * @param actionList array of UIAction objects to map to
   */
  public XMLMenuBuilder(JFrame frame, UIAction[] actionList) {
    this(frame.getClass(), actionList);
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

      // get the link tag for this file
      // node = tree.getNextElement("head");
      // get into head and get the first element
      node = 
        tree.getNextElement("head").getElementsByTagName("link").item(0);
      // node = node.getFirstChild();
      // node = node.getNextSibling();

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

    // skip to the "menubar" tag
    node = tree.getNextElement("menubar");
    current = (Element)node;
    component = new MenuBarCtrl();
    
    // iterate through every node
    node = node.getFirstChild();
    node = node.getNextSibling();
    
    // at the very first menu tag
    while (node != null) {
      processNode(node, component);
      node = node.getNextSibling();
    }
  }

  /**
   * @return the menubar built by this builder
   */
  public MenuBarCtrl getComponent() {
    return component;
  }

  public void configureForOwner(JComponent component) {
  }

  public void associateClass(Class c, Object o) {
  }

  /**
   * Process the node. This method will call <code>buildComponent</code>.
   * @param node the node to process
   * @param parent the parent component to add the information from
   * this node to
   */
  protected void processNode(Node node, JComponent parent) {
    JComponent container = null;
    JComponent item = null;

    if (node.getNodeType() != Node.ELEMENT_NODE) return; // can't process it

    // things will recurse through here
    item = buildComponent((Element)node);
    
    // find out where we stash the item
    if (item != null) {
      Element current = (Element)node;
      if (item instanceof JSeparator) {
	parent.add(item);
      } else if (parent instanceof MenuCtrl) {
	((MenuCtrl)parent).addItemByName(current.getAttribute(id_attr), 
					 (JMenuItem)item);
	System.out.println("adding " + current.getAttribute(id_attr));
	
      } else if (parent instanceof MenuBarCtrl) {
	((MenuBarCtrl)parent).addItemByName(current.getAttribute(id_attr), 
					    (JMenuItem)item);
	System.out.println("adding " + current.getAttribute(id_attr));
      }
    }
  }

  /**
   * Build the component at the current XML element and add to the parent
   * @param current the current element
   */
  protected JComponent buildComponent(Element current) {
    String tag = current.getTagName();
    JComponent comp = null;
    String label = null;;
    
    // menu tag
    if (tag.equals(menu_tag)) {
      Node node;
      JMenu menu = new MenuCtrl();
      String my_id = current.getAttribute(id_attr);
      comp = menu;

      label = getReferencedLabel(current, label_attr);
      if (label != null) ((JMenuItem)comp).setText(label);
      menu.setActionCommand(my_id);
      node = current.getFirstChild().getNextSibling();

      // loop through all its children
      while (node != null) {
	processNode(node, menu);
	node = node.getNextSibling();
      }
    } else if (tag.equals(menuitem_tag)) { // menuitem tag
      String type = current.getAttribute(type_attr);
      UIAction action;
      
      // which type of menuitem?
      if (type == null || type.equals("")) { 
        // no type ? it's a regular menuitem
	comp = buildMenuItem(current);
      } else if (type.equals(separator_attr)) { // separator
	comp = buildSeparator(current);
      } else if (type.equals(checkbox_attr)) { // checkboxes
	comp = buildCheckBoxMenuItem(current);
      } else if (type.equals(radio_attr)) { // radio
	comp = buildRadioMenuItem(current);
      }
    }

    return comp;
  }
  
  /**
   * Build a JRadioMenuItem
   * @param current the element that describes the JRadioMenuItem
   * @return the built component
   */
  protected JRadioButtonMenuItem buildRadioMenuItem(Element current) {
    String group = current.getAttribute(group_attr);
    ButtonGroup bg;
    JRadioButtonMenuItem comp = new JRadioButtonMenuItem();
    finishComponent(comp, current);
    
    // do we add to a button group?
    if (group != null) {
      if (button_group.containsKey(group)) {
	bg = (ButtonGroup)button_group.get(group);
      } else {
	bg = new ButtonGroup();
	button_group.put(group, bg);
      }
      bg.add((JRadioButtonMenuItem)comp);
    }

    return comp;
  }

  /**
   * Build a JCheckBoxMenuItem.
   * @param current the element that describes the JCheckBoxMenuItem
   * @return the built component
   */
  protected JCheckBoxMenuItem buildCheckBoxMenuItem(Element current) {
    JCheckBoxMenuItem item = new JCheckBoxMenuItem();
    finishComponent(item, current);
    return item;
  }

  /**
   * Build a JSeparator.
   * @param current the element that describes the JSeparator
   * @return the built component
   */
  protected JSeparator buildSeparator(Element current) {
    return new JSeparator();
  }

  /**
   * Build a JMenuItem.
   * @param current the element that describes the JMenuItem
   * @return the built component
   */
  protected JMenuItem buildMenuItem(Element current) {
    JMenuItem item = new JMenuItem();
    finishComponent(item, current);
    return item;
  }

  private void finishComponent(JMenuItem item, Element current) {
    String label = getReferencedLabel(current, label_attr);
    UIAction action = null;

    if (label != null) {
      item.setText(label);
    }

    label = current.getAttribute("action");
    if (label != null 
        && (action = (UIAction)actions.get(label)) != null) {
      item.addActionListener(action);
    }
    
    label = getReferencedLabel(current, accel_attr);
    if (label != null) {
      item.setMnemonic(label.charAt(0));
    }
  }

  public static void main(String[] args) throws Exception {
    javax.swing.JFrame frame = new javax.swing.JFrame("Foo bar");
    XMLMenuBuilder builder = new XMLMenuBuilder(frame, new UIAction[0]);
    URL url = builder.getClass().getResource("menus.xml");
    builder.buildFrom(url.openStream());
    frame.setJMenuBar((JMenuBar)builder.getComponent());
    frame.pack();
    frame.setVisible(true);
  }
}
