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
 * The Initial Developer of the Original Code is Jeff Galyan 
 * <talisman@anamorphic.com>.  Portions created by Jeff Galyan are 
 * Copyright (C) 1997 Jeff Galyan.  All Rights Reserved.
 *
 * Contributors: Edwin Woudt <edwin@woudt.nl>
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
import grendel.ui.UIAction;

import grendel.widgets.Animation;
import grendel.widgets.Spring;

public class GrendelToolBar extends JToolBar {
    
    private ToolBarLayout layout;

    public GrendelToolBar() {
	super();
	layout = new ToolBarLayout();
	setLayout(layout);
	setFloatable(false);
    }

    public Spring makeNewSpring() {
	return layout.createSpring();
    }

    public void addButton(UIAction aActionListener,
                          String aImageName,
                          String aText,
                          String aToolTip) {
      JButton b = new JButton();

      b.setHorizontalTextPosition(JButton.CENTER);
      b.setVerticalTextPosition(JButton.BOTTOM);
      Font f=b.getFont();
      Font nf=new Font(f.getName(), Font.PLAIN, f.getSize()-1);
      b.setFont(nf);
      b.setText(aText);

      b.setRolloverEnabled(true);
      b.setBorder(BorderFactory.createEmptyBorder());
      b.setMargin(new Insets(5,5,5,5));
      b.setToolTipText(aToolTip);
      
      System.out.println(aImageName);

      URL iconUrl = getClass().getResource("toolbar/mozilla/" + aImageName + ".gif");
      b.setIcon(new ImageIcon(iconUrl));
      //iconUrl = getClass().getResource("toolbar/mozilla/" + aImageName + "-disabled.gif");
      //b.setDisabledIcon(new ImageIcon(iconUrl));
      //iconUrl = getClass().getResource("toolbar/mozilla/" + aImageName + "-pressed.gif");
      //b.setPressedIcon(new ImageIcon(iconUrl));
      //iconUrl = getClass().getResource("toolbar/mozilla/" + aImageName + "-rollover.gif");
      //b.setRolloverIcon(new ImageIcon(iconUrl));

      Dimension d=b.getPreferredSize();
      System.out.println(d.getWidth());
      System.out.println(d.getHeight());
      double w=d.getWidth();
      if (w > 48) {
        d.setSize(w, 38);
      } else {
        d.setSize(48, 38);
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
