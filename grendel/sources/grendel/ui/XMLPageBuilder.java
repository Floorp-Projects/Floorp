/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Giao Nguyen
 * <grail@cafebabe.org>.  Portions created by Giao Nguyen are
 * Copyright (C) 1999 Giao Nguyen. All
 * Rights Reserved.
 *
 * Contributor(s): Morgan Schweers <morgan@vixen.com>
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
import javax.swing.ImageIcon;
import javax.swing.JComboBox;

import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.Document;

import javax.xml.parsers.DocumentBuilderFactory;

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
  static final String label_tag = "label";
  static final String image_tag = "image";
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
  public JComponent buildFrom(InputStream stream) {
    Document doc;
    Node node;
    URL linkURL;
    Element current;

    try {
      doc = DocumentBuilderFactory.newInstance().newDocumentBuilder().parse(stream);
      current = doc.getDocumentElement();

      // get the link tag for this file
      // get into head and get the first element
      node = current.getElementsByTagName("head").item(0).getFirstChild().getNextSibling();

      // set the configuration contained in this node
      setConfiguration((Element)node);

      // skip to the body and find the element
      current = findTargetElement((Element)current.getElementsByTagName("body").item(0));

      if (current != null) {
        component = (PageUI)buildFrom(current);
      }
    } catch (Throwable t) {
      t.printStackTrace();
    }

    return component;
  }

  /**
   * Locates the element targeted.
   * @param element the element that contains all nodes to search
   * @return the element matching the target description
   */
  private Element findTargetElement(Element element) {
    Node node = element.getFirstChild().getNextSibling();

    while (node != null) {
      // make sure we're looking at an ELEMENT_NODE
      if (node.getNodeType() == Node.ELEMENT_NODE) {
        element = (Element)node;

        // found it
        if (element.getAttribute(attr).equals(id)) break;
      }

      // proceed to the next node in the tree
      node = node.getNextSibling();
    }

    // we didn't find it otherwise we wouldn't be here.
    return element;
  }

  /**
   * Build a page from this element.
   * @param element figure it out you twit
   * @return the component built
   */
  public JComponent buildFrom(Element element) {
    Node node;
    PageUI my_component = buildPanel((Element)element.getFirstChild().getNextSibling());
    title = getReferencedLabel(element, "title");

    return my_component;
  }

  /**
   * Build a panel.
   * @param element the panel element
   * @return the panel object as a PageUI type
   */
  public PageUI buildPanel(Element element) {
    Node node = element.getFirstChild().getNextSibling();

    PageUI my_component = new PageUI();

    // every pageui in a dialog shares the same model. pageui just
    // spec the layout.
    my_component.setModel(model);

    // in the future, we'll determine what the layout manager is. for now,
    // the only one we support is GridBagLayout
    my_component.setLayout(new GridBagLayout());

    while (node != null) {
      if (node.getNodeType() == Node.ELEMENT_NODE) {
        element = (Element)node;
        JComponent child = buildComponent(element);
        GridBagConstraints cons = buildConstraints(element);
        my_component.addCtrl(element.getAttribute("ID"), child, cons);
      }

      node = node.getNextSibling();
    }

    return my_component;
  }

  private int parseGridConstant(String data) {
    int result = 1;

    try {
      data = data.trim();
      result = Integer.parseInt(data);
    } catch (NumberFormatException nfe) {
      if (data.equals("remainder")) {
        result = GridBagConstraints.REMAINDER;
      } else if (data.equals("relative")) {
        result = GridBagConstraints.RELATIVE;
      }
    } finally {
      return result;
    }
  }

  private int parseFillConstant(String fill) {
    int result = GridBagConstraints.NONE;

    fill = fill.trim();

    if (fill.equals("horizontal")) {
      result = GridBagConstraints.HORIZONTAL;
    } else if (fill.equals("vertical")) {
      result = GridBagConstraints.VERTICAL;
    } else if (fill.equals("both")) {
      result = GridBagConstraints.BOTH;
    }

    return result;
  }

  private float parseWeightConstant(String weight) {
    float result = 0.0f;

    try {
      result = Float.valueOf(weight).floatValue();
    } catch (NumberFormatException nfe) {
    } finally {
      return result;
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

    constraints.weightx =
      parseWeightConstant(current.getAttribute("weightx"));

    constraints.weighty =
      parseWeightConstant(current.getAttribute("weighty"));

    // fill
    constraints.fill = parseFillConstant(current.getAttribute("fill"));

    constraints.gridwidth =
      parseGridConstant(current.getAttribute("gridwidth"));

    constraints.gridheight =
      parseGridConstant(current.getAttribute("gridheight"));

    constraints.insets = inset;

    return constraints;
  }

  protected JComponent buildComponent(Element current) {
    JComponent item = null;
    String tag = current.getTagName();

    if (tag.equals(input_tag)) { // input tag
      item = buildInput(current);
    } else if (tag.equals(label_tag)) {
      item = buildLabel(current);
    } else if (tag.equals(panel_tag)) {
      item = buildPanel(current);
    } else if (tag.equals(image_tag)) {
      item = buildImage(current);
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
    String s = current.getAttribute("columns");
    s = s.trim();
    try {
      int column = Integer.parseInt(s);
      item.setColumns(column);
    } catch (NumberFormatException nfe) {
    }
  }

  protected JTextField buildTextField(Element current) {
    JTextField item = new JTextField();
    textFieldAdjustments(item, current);

    return item;
  }

  protected JRadioButton buildRadioButton(Element current) {
    JRadioButton item = new JRadioButton();
    String label = getReferencedLabel(current, "title");
    String group_str = current.getAttribute("group");

    if (label.length() > 0) {
      item.setText(label);
    }

    // button group matters
    if (group_str.length() > 0) {
      ButtonGroup bg;
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

  private JComponent buildInput(Element current) {
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
    } else if (type.equals("combobox")) {
      item = buildComboBox(current);
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

  protected JComboBox buildComboBox(Element current) {
    JComboBox combobox = new JComboBox();
    return combobox;
  }

  protected JLabel buildLabel(Element current) {
    JLabel label = new JLabel(getReferencedLabel(current, "title"));
    return label;
  }

  protected JLabel buildImage(Element current) {
    URL iconURL = ref.getResource(getReferencedLabel(current, "href"));
    JLabel label = new JLabel(new ImageIcon(iconURL));
    return label;
  }

  public JPanel getComponent() {
    return component;
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

    dialog.getContentPane().setLayout(new java.awt.GridBagLayout());
    dialog.getContentPane().add(panel, null);
    dialog.setTitle(builder.getTitle());
    dialog.pack();
    dialog.setVisible(true);
  }

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
