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
import java.util.Enumeration;
import java.util.Properties;
import java.util.StringTokenizer;
import java.util.NoSuchElementException;

import java.awt.Container;
import java.awt.Component;
import java.awt.GridBagLayout;
import java.awt.GridBagConstraints;
import java.awt.Insets;

import javax.swing.JComponent;
import javax.swing.JList;
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


public class PageUI extends JPanel {
  Hashtable table = new Hashtable();
  String title;

  public PageUI() {
  }

  public PageUI(URL url, String attribute, String id, PageModel model, 
                Class reference) {
    XMLPageBuilder pb = 
      new XMLPageBuilder(attribute, id, model, this, reference);
    try {
      pb.buildFrom(url.openStream());
      title = pb.getTitle();
    } catch (Exception e) {
    }
  }

  public PageUI(URL url, String attribute, String id, PageModel model) {
    XMLPageBuilder pb = 
      new XMLPageBuilder(attribute, id, model, this);
    try {
      pb.buildFrom(url.openStream());
      title = pb.getTitle();
    } catch (Exception e) {
    }
  }

  public void addCtrl(String name, JComponent component) {
    if (name != null) table.put(name, component);
    add(component);
}

  public void addCtrl(String name, JComponent component, 
			   Object constraints) {
    if (name != null) table.put(name, component);
    add(component, constraints);
  }

  /**
   * Get a component by its name.
   * 
   * @param key the name of the component to retrieve
   * @param return the component identiified by the key
   */
  public JComponent getCtrlByName(String key) {
    return (JComponent)table.get(key);
  }

  /**
   * Get the title of this page.
   *
   * @return the title for this page
   */
  public String getTitle() {
    return title;
  }


  /**
   * Store the values set in the input fields of the page.
   */
  public void saveAll() {
    System.out.println(table.size() + " elements");
  }

  /**
   * Initialize input fields to known values.
   */
  public void initAll() {
  }
}
