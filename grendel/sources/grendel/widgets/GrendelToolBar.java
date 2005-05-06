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
 * The Initial Developer of the Original Code is Jeff Galyan
 * <talisman@anamorphic.com>.  Portions created by Jeff Galyan are
 * Copyright (C) 1997 Jeff Galyan. All
 * Rights Reserved.
 *
 * Contributor(s): Edwin Woudt <edwin@woudt.nl>
 */

package grendel.widgets;

import java.awt.Dimension;
import java.awt.Font;
import java.awt.GridBagConstraints;
import java.awt.Insets;

import java.net.URL;

import javax.swing.BorderFactory;
import javax.swing.ImageIcon;
import javax.swing.JToolBar;
import javax.swing.JButton;

import grendel.ui.ToolBarLayout;

import grendel.widgets.Animation;
import grendel.widgets.Spring;

import com.trfenv.parsers.Event;

public class GrendelToolBar extends JToolBar {

  private ToolBarLayout layout;

  public GrendelToolBar() {
    super();
    layout = new ToolBarLayout();
    setLayout(layout);
    setFloatable(false);

    setBackground(new java.awt.Color(238,238,238));
  }

  public Spring makeNewSpring() {
    return layout.createSpring();
  }

  public void addButton(Event aActionListener,
                          String aImageName,
                          String aText,
                          String aToolTip) {
    JButton b = new JButton();

    b.setHorizontalTextPosition(JButton.CENTER);
    b.setVerticalTextPosition(JButton.BOTTOM);
    Font f=b.getFont();
    Font nf=new Font(f.getName(), Font.PLAIN, f.getSize()-2);
    b.setFont(nf);
    b.setText(aText);

    b.setRolloverEnabled(true);
    b.setBorder(BorderFactory.createEmptyBorder());
    b.setToolTipText(aToolTip);

    b.setIcon(new ImageIcon("widgets/toolbar/mozilla/" + aImageName + ".gif"));

    Dimension d=b.getPreferredSize();
    double w=d.getWidth();
    if (w > 52) {
      d = new Dimension((int)w, 50);
    } else {
      d = new Dimension(52, 50);
    }
    b.setMinimumSize(d);
    b.setMaximumSize(d);
    b.setPreferredSize(d);

    if (aActionListener != null) {
      b.addActionListener(aActionListener);
    } else {
      b.setEnabled(false);
    }

    add(b);

  }

}
