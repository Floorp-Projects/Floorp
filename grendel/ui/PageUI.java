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
import javax.swing.JToggleButton;
import javax.swing.JScrollPane;
import javax.swing.text.JTextComponent;

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
  PageModel model;
  String title;

  public PageUI() {
  }

  public PageUI(URL url, String attribute, String id, PageModel model, 
                Class reference) {
    XMLPageBuilder pb = 
      new XMLPageBuilder(attribute, id, model, this, reference);
    build(pb, url, model);
  }

  public PageUI(URL url, String attribute, String id, PageModel model) {
    XMLPageBuilder pb = 
      new XMLPageBuilder(attribute, id, model, this);
    build(pb, url, model);
  }

  private void build(XMLPageBuilder pb, URL url, PageModel model) {
    setModel(model);
    try {
      pb.buildFrom(url.openStream());
      title = pb.getTitle();
    } catch (Exception e) {
    }

    model.setAttribute("userNameField", "foo");
    model.setAttribute("userOrgainzationField", "bar");
    model.setAttribute("userEmailAddressField", "grail@cafebabe.org");
  }

  /**
   * Get the model for the page.
   * 
   * @return the model
   */
  public PageModel getModel() {
    return model;
  }

  /**
   * Set the model for the page.
   * 
   * @param model the model as the backing for the page
   */
  public void setModel(PageModel model) {
    this.model = model;
  }

  public void addCtrl(String name, JComponent component) {
    if (name != null) table.put(name, component);
    if (component instanceof JList) {
      component = new JScrollPane(component);
    }
    add(component);
  }

  public void addCtrl(String name, JComponent component, 
			   Object constraints) {
    if (name != null) table.put(name, component);
    if (component instanceof JList) {
      component = new JScrollPane(component);
    }
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
    Enumeration e = table.keys();

    while(e.hasMoreElements()) {
      String k = (String)e.nextElement();
      Object obj = table.get(k);
      Object val = null;

      if (obj instanceof JTextComponent) {
        JTextComponent tc = (JTextComponent)obj;
        String str = tc.getText();
        val = str;
      } else if (obj instanceof JToggleButton) {
        JToggleButton button = (JToggleButton)obj;
        Boolean b = new Boolean(button.isSelected());
        val = b;
      }

      if (val != null) {
        System.out.println(k + " is " + val);
        model.setAttribute(k, val);
      }
    }
  }

  /**
   * Initialize input fields to known values.
   */
  public void initAll() {
    Enumeration e = table.keys();

    while (e.hasMoreElements()) {
      String s = (String)e.nextElement();
      Object obj = table.get(s);
      Object val = model.getAttribute(s);

      if (obj instanceof JTextComponent) {
        JTextComponent tf = (JTextComponent)obj;
        tf.setText((String)val);
      } else if (obj instanceof JToggleButton) {
        JToggleButton button = (JToggleButton)obj;
        Boolean b = (Boolean)model.getAttribute(s);
        button.setSelected(((Boolean)val).booleanValue());
      }
    }
  }
}
