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

import javax.swing.JComponent;
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
import com.sun.xml.tree.XmlDocumentBuilder;
import com.sun.xml.tree.TreeWalker;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;
import org.xml.sax.SAXParseException;

/**
 * Build a menu bar from an XML data source. This builder supports:
 * <UL>
 * <LI>Text label cross referencing to a properties file.
 * <LI>Action lookups to an action factory of some sort.
 * </UL>
 */
public class XMLMenuBuilder {
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
  static final int ELEMENT_TYPE = 1;

  Hashtable widget_parents;
  Hashtable button_group;
  Properties properties;
  JMenuBar component;

  /**
   * Build a menu builder which operates on XML formatted data
   */
  public XMLMenuBuilder() {
    widget_parents = new Hashtable();
    button_group = new Hashtable();
  }

  /**
   * Build a menu builder which operates on XML formatted data
   *
   * @param stream the stream containing the XML data assumes
   * a single menubar in the file
   */
  public XMLMenuBuilder(InputStream stream) { 
    super();
    buildFrom(stream);
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

      // get the link tag for this file
      node = tree.getNextElement("head");
      // get into head and get the first element
      node = node.getFirstChild();
      node = node.getNextSibling();

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
	properties.load(linkURL.openStream());
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

    // skip to the "menubar" tag
    current = (Element)tree.getNextElement("menubar");
    component = new JMenuBar();
    // store its "id" so we can use it for lookups as we add its children
    widget_parents.put(current.getAttribute(id_attr), component);
    
    // iterate through every node
    node = tree.getNext();
    while (node != null) {
      processNode(node);
      node = tree.getNext();
    }
  }

  /**
   * @return the menubar built by this builder
   */
  public JComponent getComponent() {
    return component;
  }

  public void configureForOwner(JComponent component) {
  }

  public void associateClass(Class c, Object o) {
  }

  protected void processNode(Node node) {
    JComponent container = null;
    JComponent item = null;

    if (node.getNodeType() == ELEMENT_TYPE) {
      item = buildComponent((Element)node);

      // find out where we stash the item
      if (item != null) {
	Element current = (Element)node;
	Element parent = (Element)current.getParentNode();
	String label;

	// look up the parent container and add it there
	container = 
	  (JComponent)widget_parents.get(parent.getAttribute(id_attr));
	label = getReferencedLabel(current, label_attr);
	if (label != null) ((JMenuItem)item).setText(label);
	label = getReferencedLabel(current, accel_attr);
	if (label != null) ((JMenuItem)item).setMnemonic(label.charAt(0));
	container.add(item);
      }
    }
  }

  protected JComponent buildComponent(Element current) {
    String tag = current.getTagName();
    JComponent comp = null;
    
    // menu tag
    if (tag.equals(menu_tag)) {
      String my_id = current.getAttribute(id_attr);
      comp = new JMenu();
      widget_parents.put(my_id, comp);
    } else if (tag.equals(menuitem_tag)) { // menuitem tag
      String type = current.getAttribute(type_attr);

      if (type == null) { // no type ? it's a regular menuitem
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

  protected JRadioButtonMenuItem buildRadioMenuItem(Element current) {
    String group = current.getAttribute(group_attr);
    ButtonGroup bg;
    JRadioButtonMenuItem comp = new JRadioButtonMenuItem();
    
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

  protected JCheckBoxMenuItem buildCheckBoxMenuItem(Element current) {
    return new JCheckBoxMenuItem();
  }

  protected JSeparator buildSeparator(Element current) {
    return new JSeparator();
  }

  protected JMenuItem buildMenuItem(Element current) {
    return new JMenuItem();
  }

  protected String getReferencedLabel(Element current, String attr) {
    String label = current.getAttribute(attr);
    
    if (properties == null) return label;

    // if it starts with a '$' we crossreference to properties
    if (label != null && label.charAt(0) == '$') {
      String key = label.substring(1);
      label = properties.getProperty(key, label);
    }

    return label;
  }

  public static void main(String[] args) throws Exception {
    javax.swing.JFrame frame = new javax.swing.JFrame("Foo bar");
    XMLMenuBuilder builder = new XMLMenuBuilder();
    URL url = builder.getClass().getResource("menus.xml");
    builder.buildFrom(url.openStream());
    frame.setJMenuBar((JMenuBar)builder.getComponent());
    frame.pack();
    frame.setVisible(true);
  }
}
