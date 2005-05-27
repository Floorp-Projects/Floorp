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
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Created: Will Scullin <scullin@netscape.com>, 13 Oct 1997.
 *
 * Contributors: Jeff Galyan <talisman@anamorphic.com>
 *               Edwin Woudt <edwin@woudt.nl>
 */

package grendel.ui;

import grendel.widgets.GrendelToolBar;

import java.awt.BorderLayout;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.Image;
import java.awt.datatransfer.Clipboard;
import java.util.Enumeration;
import java.util.Hashtable;
import java.util.Locale;
import java.util.MissingResourceException;
import java.util.ResourceBundle;
import java.util.StringTokenizer;

import javax.swing.BorderFactory;
import javax.swing.Icon;
import javax.swing.ImageIcon;
import javax.swing.JPanel;
import javax.swing.JButton;
import javax.swing.JToolBar;

import com.trfenv.parsers.Event;
import com.trfenv.parsers.xul.XulParser;

import org.w3c.dom.Element;
import org.w3c.dom.Document;

public class GeneralPanel extends JPanel {
  private final boolean DEBUG = false;
  static ResourceBundle fLabels = ResourceBundle.getBundle("grendel.ui.Labels",
                                                         Locale.getDefault());

  static Clipboard fPrivateClipboard = new Clipboard("Grendel");

  protected String         fResourceBase = "grendel.ui";
  protected JToolBar fToolBar;

  public GeneralPanel() {
    setLayout(new BorderLayout());
    setFont(new Font("Helvetica", Font.PLAIN, 12));
  }

  public Event[] getActions() {
    return null;
  }

  protected JToolBar buildToolBar(String aToolbar, Event[] aActions) {
    XulParser curParser = new XulParser(aActions, null);
    Document doc = curParser.makeDocument("ui/grendel.xml");
    Element toolbar = (Element)doc.getDocumentElement().getElementsByTagName("toolbar").
        item(0);

    JToolBar newToolbar = (JToolBar)curParser.parseTag(this, toolbar);

    return newToolbar;
  }

  public JToolBar getToolBar() {
    return fToolBar;
  }
}
