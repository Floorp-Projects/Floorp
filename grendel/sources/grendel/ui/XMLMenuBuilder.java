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
 * Contributor(s):
 *   Morgan Schweers <morgan@vixen.com>
 *   R.J. Keller <rj.keller@beonex.com>
 */

package grendel.ui;

import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;

import java.util.Map;
import java.util.HashMap;
import java.util.logging.*;

import javax.swing.JMenuBar;
import javax.swing.JComponent;
import javax.swing.JFrame;

import org.w3c.dom.Element;
import org.w3c.dom.Document;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;

import com.trfenv.parsers.xul.XulParser;
import com.trfenv.parsers.Event;

/**
 * Build a menu bar from an XML data source. This builder supports:
 * <UL>
 * <LI>Text label cross referencing to a DTD file.
 * <LI>Action lookups.
 * </UL>
 */
public class XMLMenuBuilder {
  private Event[] mListener;
  private Element root;
  private JMenuBar mMenuBar;

  private Logger logger = Logger.getLogger("grendel.ui.XMLMenuBuilder");

  private Map<String, JComponent> mIDs;

  /**
   * Build a menu builder which operates on XML formatted data
   *
   * @param aListener The action listener that contains the events
   *   for all the tags that will be parsed.
   */
  public XMLMenuBuilder(Event[] aListener) {
    mListener = aListener;
    mIDs = new HashMap<String, JComponent>();
  }

  /**
   *Builds a JMenuBar based on the input stream data.
   *
   * @param stream The input stream to read the XML data from.
   * @param aWindow The parent window of this menubar.
   */
  public JMenuBar buildFrom(String file, JFrame aWindow) {
    XulParser parser = new XulParser(mListener, aWindow);

    logger.info("Parsing XML file: " + file);

    Document doc = XulParser.makeDocument(file);
    root = doc.getDocumentElement();

    //no, we don't support multiple menu bars. If there are multiple menubars, just take
    //the first one.
    Element menubarTag = (Element)root.getElementsByTagName("menubar").item(0);
    mMenuBar = (JMenuBar)parser.parseTag(null, menubarTag);
    mIDs = parser.getIDs();

    logger.info("Parsing Successful!");

    return mMenuBar;
  }

  /**
   *Returns a map containing the element ID as a string and the JComponent being
   *the component that the ID connects to.
   */
  public Map<String, JComponent> getElementsAndIDs()
  {
    return mIDs;
  }
}
