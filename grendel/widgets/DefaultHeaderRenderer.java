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
 * Created: Will Scullin <scullin@netscape.com>,  2 Oct 1997.
 * Modified: Jeff Galyan <jeffrey.galyan@sun.com>, 31 Dec 1998
 */

package grendel.widgets;

import java.awt.Color;
import java.awt.Component;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.FontMetrics;
import java.awt.Graphics;
import java.awt.Insets;

import javax.swing.BorderFactory;
import javax.swing.Icon;
import javax.swing.JComponent;
import javax.swing.UIManager;
import javax.swing.plaf.basic.BasicGraphicsUtils;
import javax.swing.border.BevelBorder;
import javax.swing.border.Border;
import javax.swing.border.EmptyBorder;

/**
 * The default renderer for column headers. Handles text headers and
 * not much else. Aspires to handle icons.
 */

public class DefaultHeaderRenderer extends JComponent implements HeaderRenderer {
  Column  fColumn;
  int     fState;
  Border  fRaisedBorder;
  Border  fLoweredBorder;
  Border  fEmptyBorder;

  public DefaultHeaderRenderer() {
    fRaisedBorder = BorderFactory.createRaisedBevelBorder();
    fLoweredBorder = BorderFactory.createLoweredBevelBorder();

    fEmptyBorder = new EmptyBorder(0,0,0,0);

    //setFont(Font.decode("Dialog-12"));
    setFont(UIManager.getFont("Label.font"));
    setBorder(fRaisedBorder);
  }

  // HeaderRenderer Interface

  public void setValue(Column aColumn, int aState) {
    fColumn = aColumn;
    fState = aState;

    setSize(getPreferredSize());
  }

  public Component getComponent() {
    return this;
  }

  // Component overloads

  public Dimension getPreferredSize() {
    Font font = getFont();
    String text = fColumn.getTitle();
    Icon icon = fColumn.getIcon();

    Insets insets = getInsets();
    Dimension res = new Dimension(fColumn.getWidth(), 0);

    if (font != null && text != null) {
      FontMetrics fm = getToolkit().getFontMetrics(font);
      res.height = fm.getHeight();
    }

    if (icon != null && icon.getIconHeight() > res.height) {
      res.height = icon.getIconHeight();
    }

    res.height += 2 + insets.top + insets.bottom;

    return res;
  }

  public Dimension getMinimumSize() {
    return getPreferredSize();
  }

  public Dimension getMaximumSize() {
    return getPreferredSize();
  }

  public void paint(Graphics g) {
    Dimension size = getSize();

    if (fState == HeaderRenderer.DRAGGING) {
      g.fillRect(0, 0, size.width, size.height);
      setBorder(fEmptyBorder);
    } else if (fState == HeaderRenderer.DEPRESSED) {
      setBorder(fLoweredBorder);
    } else {
      setBorder(fRaisedBorder);
    }

    Font font = getFont();
    String text = fColumn.getTitle();
    Icon icon = fColumn.getIcon();
    Insets insets = getInsets();

    int x = insets.left + 2;
    int y = 0;
    int h = size.height;

    if (fState == HeaderRenderer.DEPRESSED) {
      x++;
      y++;
    }

    if (icon != null) {
      int iconY = y + (h - icon.getIconHeight()) / 2;

      icon.paintIcon(this, g, x, iconY);

      x += icon.getIconWidth() + 2;
    }

    if (font != null && text != null) {
      g.setFont(font);
      FontMetrics fm = getToolkit().getFontMetrics(font);
      int textY = y + (h - fm.getHeight()) / 2 + fm.getAscent();

      if (fState == HeaderRenderer.SELECTED) {
        g.setColor(UIManager.getColor("textHighlightText"));
      } else {
        g.setColor(UIManager.getColor("textText"));
      }

      g.drawString(text, x, textY);
    }
    super.paint(g);
  }
}

